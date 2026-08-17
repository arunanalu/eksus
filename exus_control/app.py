"""Aplicativo Tkinter: bancada BLE e painel da ponte no mesmo processo."""

from __future__ import annotations

import asyncio
import concurrent.futures
import queue
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
from pathlib import Path
import shutil

from .ble_client import ExusDevice, scan
from .bridge_server import BridgeServer
from .events import SCHEMA
from .logging import SessionLogger
from .session import BridgeSession
from .transports import Capabilities, MockTransport
from .transports.ble import BleTransportAdapter


class ControlWorker:
    def __init__(self):
        self.loop = asyncio.new_event_loop()
        self.thread = threading.Thread(target=self._run, daemon=True, name="exus-control")
        log_name = f"session-{time.strftime('%Y%m%d-%H%M%S')}.jsonl"
        simulation_transport = MockTransport(Capabilities((0,), "Simulador"), connected=False)
        self.bridge_session = BridgeSession(simulation_transport, simulation_transport.capabilities,
                                            logger=SessionLogger(Path.cwd() / "logs" / log_name))
        self.bridge = BridgeServer(self.bridge_session)
        self.adapter: BleTransportAdapter | None = None
        self.thread.start()

    def _run(self):
        asyncio.set_event_loop(self.loop); self.loop.run_forever()

    def submit(self, coroutine) -> concurrent.futures.Future:
        return asyncio.run_coroutine_threadsafe(coroutine, self.loop)

    async def scan(self): return await scan()

    async def connect(self, device: ExusDevice):
        await self.disconnect()
        self.adapter = BleTransportAdapter(device)
        caps = await self.adapter.connect()
        await self.bridge_session.set_transport(self.adapter, caps)
        return await self.adapter.info(), list(caps.zones_ready)

    async def command(self, command: str):
        if not self.adapter: raise RuntimeError("Conecte um protótipo primeiro.")
        return (await self.adapter.send(command)).response

    async def emergency(self): await self.bridge_session.emergency()

    async def disconnect(self):
        await self.bridge_session.set_hardware_output(False)
        await self.bridge_session.on_disconnect()
        if self.adapter: await self.adapter.disconnect()
        self.adapter = None

    async def is_connected(self):
        connected = bool(self.adapter and self.adapter.state.value == "connected")
        if not connected:
            await self.bridge_session.on_disconnect()
        return connected
    async def start_bridge(self): await self.bridge.start(); return self.bridge_snapshot()
    async def stop_bridge(self): await self.bridge.stop(); return self.bridge_snapshot()
    async def set_output(self, enabled: bool): return await self.bridge_session.set_hardware_output(enabled)
    async def bridge_snapshot(self):
        stats = self.bridge.stats
        return {"listening": self.bridge.listening, "output": self.bridge_session.hardware_output_enabled,
                "stats": stats, "last_error": self.bridge.last_error}
    async def manual_event(self, event: str = "damage"):
        payload = {"schema": SCHEMA, "session_id": "manual", "seq": self.bridge.stats.received + 1,
                   "sent_at_ms": 0, "event": event, "state": "oneshot", "stream_id": None,
                   "azimuth_deg": 0, "magnitude": .4, "duration_ms": 80, "source": "manual", "output_requested": False}
        return await self.bridge_session.handle_payload(payload)

    def close(self):
        try: self.submit(self._close()).result(timeout=3)
        except Exception: pass
        self.loop.call_soon_threadsafe(self.loop.stop)
    async def _close(self):
        await self.bridge.stop(); await self.disconnect()


class ExusControl(ttk.Frame):
    BACKGROUND = "#0b1120"
    SURFACE = "#111827"
    SURFACE_ALT = "#1f2937"
    BORDER = "#334155"
    TEXT = "#e5e7eb"
    MUTED = "#94a3b8"
    ACCENT = "#2563eb"
    ACCENT_HOVER = "#3b82f6"

    def __init__(self, root: tk.Tk):
        self.root = root; self._configure_theme()
        super().__init__(root, padding=14, style="App.TFrame"); self.worker = ControlWorker()
        self.devices: list[ExusDevice] = []; self.zone_vars: dict[int, tk.BooleanVar] = {}; self.events = queue.Queue()
        self.connection_active = False; self.device_choice = tk.StringVar(); self.connection_text = tk.StringVar(value="NÃO CONECTADO")
        self.info_text = tk.StringVar(value="Modo simulado disponível sem protótipo.")
        self.intensity, self.duration, self.frequency = tk.IntVar(value=15), tk.IntVar(value=500), tk.IntVar(value=10)
        self.bridge_text, self.bridge_detail, self.output_enabled = tk.StringVar(value="PARADA"), tk.StringVar(value="127.0.0.1:4242"), tk.BooleanVar(value=False)
        self._build(); self._set_controls(False); self.root.after(100, self._poll_events); self.root.after(1000, self._health_check)

    def _configure_theme(self):
        self.root.configure(background=self.BACKGROUND)
        style = ttk.Style(self.root)
        style.theme_use("clam")
        style.configure(".", background=self.BACKGROUND, foreground=self.TEXT, font=("Segoe UI", 10))
        style.configure("App.TFrame", background=self.BACKGROUND)
        style.configure("TFrame", background=self.SURFACE)
        style.configure("TLabelframe", background=self.SURFACE, bordercolor=self.BORDER, lightcolor=self.BORDER, darkcolor=self.BORDER)
        style.configure("TLabelframe.Label", background=self.SURFACE, foreground=self.TEXT, font=("Segoe UI Semibold", 10))
        style.configure("TLabel", background=self.SURFACE, foreground=self.TEXT)
        style.configure("TButton", background=self.SURFACE_ALT, foreground=self.TEXT, bordercolor=self.BORDER, lightcolor=self.BORDER, darkcolor=self.BORDER, padding=(10, 6))
        style.map("TButton", background=[("active", "#334155"), ("disabled", "#172033")], foreground=[("disabled", "#64748b")])
        style.configure("Accent.TButton", background=self.ACCENT, foreground="#ffffff", bordercolor=self.ACCENT, padding=(10, 6))
        style.map("Accent.TButton", background=[("active", self.ACCENT_HOVER), ("disabled", "#1e3a5f")], foreground=[("disabled", "#94a3b8")])
        style.configure("TCheckbutton", background=self.SURFACE, foreground=self.TEXT, indicatorcolor=self.SURFACE_ALT, padding=3)
        style.map("TCheckbutton", background=[("active", self.SURFACE)], foreground=[("disabled", "#64748b")])
        style.configure("TCombobox", fieldbackground=self.SURFACE_ALT, background=self.SURFACE_ALT, foreground=self.TEXT, bordercolor=self.BORDER, arrowcolor=self.TEXT, padding=5)
        style.map("TCombobox", fieldbackground=[("readonly", self.SURFACE_ALT)], foreground=[("readonly", self.TEXT)], selectbackground=[("readonly", self.ACCENT)], selectforeground=[("readonly", "#ffffff")])
        style.configure("TSpinbox", fieldbackground=self.SURFACE_ALT, background=self.SURFACE_ALT, foreground=self.TEXT, bordercolor=self.BORDER, arrowcolor=self.TEXT, padding=4)
        style.map("TSpinbox", fieldbackground=[("readonly", self.SURFACE_ALT)], foreground=[("disabled", "#64748b")])

    def _build(self):
        self.root.title("Exus Control"); self.root.minsize(760, 700); self.pack(fill="both", expand=True); self.columnconfigure(0, weight=1)
        connection = ttk.LabelFrame(self, text="1. Conexão", padding=10); connection.grid(row=0, column=0, sticky="ew"); connection.columnconfigure(1, weight=1)
        self.search_button = ttk.Button(connection, text="Procurar protótipos", command=self.search, style="Accent.TButton"); self.search_button.grid(row=0,column=0,padx=(0,8))
        self.device_list = ttk.Combobox(connection,textvariable=self.device_choice,state="readonly"); self.device_list.grid(row=0,column=1,sticky="ew",padx=(0,8))
        self.connect_button = ttk.Button(connection,text="Conectar",command=self.connect, style="Accent.TButton"); self.connect_button.grid(row=0,column=2,padx=(0,8))
        self.disconnect_button = ttk.Button(connection,text="Desconectar",command=self.disconnect); self.disconnect_button.grid(row=0,column=3)
        ttk.Label(connection,textvariable=self.connection_text,font=("Segoe UI",10,"bold")).grid(row=1,column=0,columnspan=4,sticky="w",pady=(8,0))
        status = ttk.LabelFrame(self,text="2. Estado do protótipo",padding=10); status.grid(row=1,column=0,sticky="ew",pady=(10,0)); ttk.Label(status,textvariable=self.info_text,wraplength=700).pack(anchor="w")
        self.zones_frame = ttk.LabelFrame(self,text="3. Zonas",padding=10); self.zones_frame.grid(row=2,column=0,sticky="ew",pady=(10,0)); self.zones_content=ttk.Frame(self.zones_frame); self.zones_content.pack(anchor="w")
        test=ttk.LabelFrame(self,text="4. Teste seguro",padding=10); test.grid(row=3,column=0,sticky="ew",pady=(10,0))
        for col,label,var,maxval in ((0,"Intensidade (%)",self.intensity,50),(2,"Duração (ms)",self.duration,2000),(4,"Ritmo (Hz)",self.frequency,100)):
            ttk.Label(test,text=label).grid(row=0,column=col,sticky="w"); ttk.Spinbox(test,from_=1,to=maxval,textvariable=var,width=7).grid(row=0,column=col+1,sticky="w",padx=(4,14))
        self.test_button=ttk.Button(test,text="Testar zonas marcadas",command=self.test_zones, style="Accent.TButton"); self.test_button.grid(row=1,column=0,columnspan=3,sticky="ew",pady=(8,0))
        self.stop_button=ttk.Button(test,text="PARAR TUDO",command=self.stop_all); self.stop_button.grid(row=1,column=3,columnspan=3,sticky="ew",padx=(8,0),pady=(8,0))
        bridge=ttk.LabelFrame(self,text="5. Ponte de jogo",padding=10); bridge.grid(row=4,column=0,sticky="ew",pady=(10,0)); bridge.columnconfigure(1,weight=1)
        ttk.Label(bridge,textvariable=self.bridge_text,font=("Segoe UI",10,"bold")).grid(row=0,column=0,sticky="w"); ttk.Label(bridge,textvariable=self.bridge_detail).grid(row=0,column=1,sticky="w")
        self.bridge_button=ttk.Button(bridge,text="Iniciar ponte",command=self.toggle_bridge, style="Accent.TButton"); self.bridge_button.grid(row=1,column=0,sticky="ew",pady=(8,0))
        self.output_check=ttk.Checkbutton(bridge,text="Permitir saída para o protótipo",variable=self.output_enabled,command=self.toggle_output); self.output_check.grid(row=1,column=1,sticky="w",padx=(12,0),pady=(8,0))
        ttk.Button(bridge,text="Gerar dano simulado",command=lambda:self._future(self.worker.submit(self.worker.manual_event()),"manual")).grid(row=2,column=0,sticky="ew",pady=(8,0))
        ttk.Button(bridge,text="Exportar log da sessão",command=self.export_log).grid(row=2,column=1,sticky="w",padx=(12,0),pady=(8,0))
        emergency=tk.Button(self,text="EMERGÊNCIA — PARAR AGORA",bg="#b91c1c",fg="white",activebackground="#ef4444",activeforeground="white",relief="flat",bd=0,highlightthickness=0,font=("Segoe UI",13,"bold"),command=self.emergency); emergency.grid(row=5,column=0,sticky="ew",pady=(12,8)); self.emergency_button=emergency
        self.resume_button=ttk.Button(self,text="Liberar emergência após inspeção",command=self.resume); self.resume_button.grid(row=6,column=0,sticky="ew")
        log_frame=ttk.LabelFrame(self,text="Diagnóstico",padding=6); log_frame.grid(row=7,column=0,sticky="nsew",pady=(10,0)); self.rowconfigure(7,weight=1); self.log=tk.Text(log_frame,height=9,state="disabled",wrap="word",background="#060b16",foreground=self.TEXT,insertbackground=self.TEXT,selectbackground=self.ACCENT,selectforeground="#ffffff",relief="flat",bd=0,highlightthickness=1,highlightbackground=self.BORDER,highlightcolor=self.ACCENT); self.log.pack(fill="both",expand=True)

    def _set_controls(self, enabled):
        state="normal" if enabled else "disabled"
        for control in (self.test_button,self.stop_button,self.emergency_button,self.resume_button,self.disconnect_button): control.configure(state=state)
        self.connect_button.configure(state="disabled" if enabled else "normal"); self.search_button.configure(state="disabled" if enabled else "normal")
    def _log(self,msg): self.log.configure(state="normal"); self.log.insert("end",msg.strip()+"\n"); self.log.see("end"); self.log.configure(state="disabled")
    def _future(self,future,action):
        def done(result):
            try:self.events.put((action,result.result()))
            except Exception as exc:self.events.put(("error",f"{action}: {exc}"))
        future.add_done_callback(done)
    def _poll_events(self):
        while True:
            try: action,value=self.events.get_nowait()
            except queue.Empty: break
            if action=="error": self._log(f"ERRO: {value}"); self._set_controls(False); self.connection_text.set("NÃO CONECTADO"); self.output_enabled.set(False)
            elif action=="scan":
                self.devices=value; labels=[f"{item.name}  ({item.rssi} dBm)" for item in value]; self.device_list["values"]=labels
                if labels:self.device_choice.set(labels[0]); self.info_text.set("Selecione um protótipo e clique em Conectar.")
                else:self.info_text.set("Nenhum Exus encontrado. O modo simulado continua disponível.")
            elif action=="connect":
                info,zones=value; self.connection_text.set("CONECTADO — pronto para teste"); self.info_text.set(info); self._show_zones(zones); self._set_controls(True); self.connection_active=True; self._log(f"Conectado. Zonas prontas: {zones}")
            elif action=="disconnect": self.connection_text.set("NÃO CONECTADO"); self._set_controls(False); self.connection_active=False; self.output_enabled.set(False); self._log("Desconectado; saída real desabilitada.")
            elif action=="health" and not value and self.connection_active:
                self.connection_text.set("CONEXÃO PERDIDA"); self.info_text.set("Bluetooth desconectou; saída real desabilitada."); self._set_controls(False); self.connection_active=False; self.output_enabled.set(False); self._log("Conexão BLE perdida; pendências descartadas.")
            elif action=="reply": self._log(str(value))
            elif action=="manual": self._log(f"WOULD_SEND: {value.command} ({value.reason})")
            elif action=="bridge": self._update_bridge(value)
            elif action=="output":
                if not value:self.output_enabled.set(False); self._log("Saída real exige BLE conectado e zonas carregadas.")
        self.root.after(100,self._poll_events)
    def _update_bridge(self,snapshot):
        self.bridge_text.set("ESCUTANDO" if snapshot["listening"] else "PARADA"); self.bridge_button.configure(text="Parar ponte" if snapshot["listening"] else "Iniciar ponte")
        stats=snapshot["stats"]; mode="SAÍDA REAL HABILITADA" if snapshot["output"] else "MODO SIMULADO"; self.bridge_detail.set(f"127.0.0.1:4242 — {mode} — recebidos {stats.received}, simulados {stats.simulated}, enviados {stats.sent}, rejeitados {stats.rejected} — último: {stats.last_event} / {stats.last_command}")
    def _health_check(self):
        if self.connection_active:self._future(self.worker.submit(self.worker.is_connected()),"health")
        self._future(self.worker.submit(self.worker.bridge_snapshot()),"bridge"); self.root.after(1000,self._health_check)
    def search(self): self._future(self.worker.submit(self.worker.scan()),"scan")
    def connect(self):
        try: device=self.devices[list(self.device_list["values"]).index(self.device_choice.get())]
        except (ValueError,IndexError): messagebox.showwarning("Exus Control","Procure e selecione um protótipo primeiro."); return
        self.connection_text.set("CONECTANDO / PAREANDO..."); self._future(self.worker.submit(self.worker.connect(device)),"connect")
    def disconnect(self): self._future(self.worker.submit(self.worker.disconnect()),"disconnect")
    def _show_zones(self,zones):
        for widget in self.zones_content.winfo_children():widget.destroy()
        self.zone_vars={}
        for index,zone in enumerate(zones):
            variable=tk.BooleanVar(value=False); self.zone_vars[zone]=variable; ttk.Checkbutton(self.zones_content,text=f"Zona {zone}",variable=variable).grid(row=index//6,column=index%6,sticky="w",padx=(0,16),pady=2)
    def test_zones(self):
        mask=sum(1<<zone for zone,var in self.zone_vars.items() if var.get())
        if not mask:messagebox.showwarning("Exus Control","Marque ao menos uma zona."); return
        values=(self.intensity.get(),self.duration.get(),self.frequency.get())
        if not(1<=values[0]<=50 and 1<=values[1]<=2000 and 1<=values[2]<=100):messagebox.showwarning("Exus Control","Valores permitidos: 1–50%, 1–2000 ms e 1–100 Hz."); return
        self._future(self.worker.submit(self.worker.command(f"group 0x{mask:X} pulse {values[0]} {values[1]} {values[2]}")),"reply")
    def stop_all(self): self._future(self.worker.submit(self.worker.command("stop all")),"reply")
    def emergency(self):
        if messagebox.askyesno("Emergência","Enviar parada imediata para todas as zonas?"):self._future(self.worker.submit(self.worker.emergency()),"reply")
    def resume(self):
        if messagebox.askyesno("Liberar emergência","A montagem foi inspecionada e é seguro liberar a emergência?"):self._future(self.worker.submit(self.worker.command("resume")),"reply")
    def toggle_bridge(self):
        action=self.worker.stop_bridge() if self.bridge_text.get()=="ESCUTANDO" else self.worker.start_bridge(); self._future(self.worker.submit(action),"bridge")
    def toggle_output(self): self._future(self.worker.submit(self.worker.set_output(self.output_enabled.get())),"output")
    def export_log(self):
        source = self.worker.bridge_session.logger.path
        if not source or not source.exists():
            messagebox.showinfo("Exus Control", "Ainda não há eventos registrados nesta sessão."); return
        destination = filedialog.asksaveasfilename(defaultextension=".jsonl", initialfile=source.name,
                                                   filetypes=[("Log JSON Lines", "*.jsonl")])
        if destination:
            shutil.copy2(source, destination); self._log(f"Log exportado: {destination}")
    def close(self): self.worker.close(); self.root.destroy()


def main():
    root=tk.Tk(); app=ExusControl(root); root.protocol("WM_DELETE_WINDOW",app.close); root.mainloop()
