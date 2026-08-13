#!/usr/bin/env python3
"""Exus Control: interface visual de bancada para o firmware BLE Exus."""

from __future__ import annotations

import asyncio
import concurrent.futures
import queue
import threading
import tkinter as tk
from tkinter import messagebox, ttk

from bleak.exc import BleakError

from exus_ble_client import ExusBleClient, ExusDevice, parse_capabilities, scan


class BleWorker:
    """Mantém BLE em um loop próprio, para que a janela Tkinter não congele."""

    def __init__(self):
        self.loop = asyncio.new_event_loop()
        self.client: ExusBleClient | None = None
        self.thread = threading.Thread(target=self._run, daemon=True, name="exus-ble")
        self.thread.start()

    def _run(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def submit(self, coroutine) -> concurrent.futures.Future:
        return asyncio.run_coroutine_threadsafe(coroutine, self.loop)

    async def scan(self) -> list[ExusDevice]:
        return await scan()

    async def connect(self, device: ExusDevice) -> tuple[str, list[int]]:
        await self.disconnect()
        self.client = ExusBleClient(device)
        await self.client.connect(pair=True)
        info = await self.client.info()
        capabilities = await self.client.command("Q 0")
        zones = parse_capabilities(capabilities)
        if not zones:
            await self.disconnect()
            raise RuntimeError("O dispositivo conectou, mas não informou nenhuma zona pronta.")
        return info, zones

    async def command(self, command: str) -> str:
        if not self.client:
            raise RuntimeError("Conecte um protótipo primeiro.")
        return await self.client.command(command)

    async def emergency(self) -> None:
        if not self.client:
            raise RuntimeError("Conecte um protótipo primeiro.")
        await self.client.emergency()

    async def disconnect(self) -> None:
        if self.client:
            await self.client.disconnect()
            self.client = None

    async def is_connected(self) -> bool:
        return bool(self.client and self.client.connected)

    def close(self):
        try:
            self.submit(self.disconnect()).result(timeout=3)
        except (RuntimeError, concurrent.futures.TimeoutError, BleakError):
            pass
        self.loop.call_soon_threadsafe(self.loop.stop)


class ExusControl(ttk.Frame):
    SAFE_INTENSITY = 15
    SAFE_DURATION = 500
    SAFE_FREQUENCY = 10

    def __init__(self, root: tk.Tk):
        super().__init__(root, padding=14)
        self.root = root
        self.worker = BleWorker()
        self.devices: list[ExusDevice] = []
        self.zone_vars: dict[int, tk.BooleanVar] = {}
        self.events: queue.Queue[tuple[str, object]] = queue.Queue()
        self.connection_active = False
        self.device_choice = tk.StringVar()
        self.connection_text = tk.StringVar(value="NÃO CONECTADO")
        self.info_text = tk.StringVar(value="Procure um protótipo Exus para começar.")
        self.intensity = tk.IntVar(value=self.SAFE_INTENSITY)
        self.duration = tk.IntVar(value=self.SAFE_DURATION)
        self.frequency = tk.IntVar(value=self.SAFE_FREQUENCY)
        self._build()
        self._set_controls(False)
        self.root.after(100, self._poll_events)
        self.root.after(1000, self._health_check)

    def _build(self):
        self.root.title("Exus Control")
        self.root.minsize(700, 560)
        self.pack(fill="both", expand=True)
        self.columnconfigure(0, weight=1)
        self.rowconfigure(4, weight=1)

        connection = ttk.LabelFrame(self, text="1. Conexão", padding=10)
        connection.grid(row=0, column=0, sticky="ew")
        connection.columnconfigure(1, weight=1)
        self.search_button = ttk.Button(connection, text="Procurar protótipos", command=self.search)
        self.search_button.grid(row=0, column=0, padx=(0, 8))
        self.device_list = ttk.Combobox(connection, textvariable=self.device_choice, state="readonly")
        self.device_list.grid(row=0, column=1, sticky="ew", padx=(0, 8))
        self.connect_button = ttk.Button(connection, text="Conectar", command=self.connect)
        self.connect_button.grid(row=0, column=2, padx=(0, 8))
        self.disconnect_button = ttk.Button(connection, text="Desconectar", command=self.disconnect)
        self.disconnect_button.grid(row=0, column=3)
        ttk.Label(connection, textvariable=self.connection_text, font=("Segoe UI", 10, "bold")).grid(
            row=1, column=0, columnspan=4, sticky="w", pady=(8, 0)
        )

        status = ttk.LabelFrame(self, text="2. Estado do protótipo", padding=10)
        status.grid(row=1, column=0, sticky="ew", pady=(10, 0))
        ttk.Label(status, textvariable=self.info_text, wraplength=650).pack(anchor="w")

        self.zones_frame = ttk.LabelFrame(self, text="3. Zonas: marque uma ou mais", padding=10)
        self.zones_frame.grid(row=2, column=0, sticky="ew", pady=(10, 0))
        self.zones_content = ttk.Frame(self.zones_frame)
        self.zones_content.pack(anchor="w")

        test = ttk.LabelFrame(self, text="4. Teste seguro", padding=10)
        test.grid(row=3, column=0, sticky="ew", pady=(10, 0))
        for column in (1, 3, 5):
            test.columnconfigure(column, weight=1)
        ttk.Label(test, text="Intensidade (%)").grid(row=0, column=0, sticky="w")
        ttk.Spinbox(test, from_=1, to=50, textvariable=self.intensity, width=7).grid(row=0, column=1, sticky="w", padx=(4, 14))
        ttk.Label(test, text="Duração (ms)").grid(row=0, column=2, sticky="w")
        ttk.Spinbox(test, from_=1, to=2000, increment=100, textvariable=self.duration, width=7).grid(row=0, column=3, sticky="w", padx=(4, 14))
        ttk.Label(test, text="Ritmo (Hz)").grid(row=0, column=4, sticky="w")
        ttk.Spinbox(test, from_=1, to=100, textvariable=self.frequency, width=7).grid(row=0, column=5, sticky="w", padx=(4, 0))
        ttk.Label(test, text="Comece com 15%, 500 ms e 10 Hz. A placa ainda aplica seus próprios limites.").grid(
            row=1, column=0, columnspan=6, sticky="w", pady=(8, 8)
        )
        self.test_button = ttk.Button(test, text="Testar zonas marcadas", command=self.test_zones)
        self.test_button.grid(row=2, column=0, columnspan=3, sticky="ew", padx=(0, 5))
        self.stop_button = ttk.Button(test, text="PARAR TUDO", command=self.stop_all)
        self.stop_button.grid(row=2, column=3, columnspan=3, sticky="ew", padx=(5, 0))

        emergency = tk.Button(self, text="EMERGÊNCIA — PARAR AGORA", bg="#b91c1c", fg="white", activebackground="#991b1b",
            activeforeground="white", font=("Segoe UI", 13, "bold"), relief="flat", command=self.emergency)
        emergency.grid(row=4, column=0, sticky="ew", pady=(12, 8))
        self.emergency_button = emergency
        self.resume_button = ttk.Button(self, text="Liberar emergência após inspeção", command=self.resume)
        self.resume_button.grid(row=5, column=0, sticky="ew")

        log_frame = ttk.LabelFrame(self, text="Diagnóstico", padding=6)
        log_frame.grid(row=6, column=0, sticky="nsew", pady=(10, 0))
        self.rowconfigure(6, weight=1)
        self.log = tk.Text(log_frame, height=8, state="disabled", wrap="word")
        self.log.pack(fill="both", expand=True)

    def _set_controls(self, enabled: bool):
        state = "normal" if enabled else "disabled"
        for control in (self.test_button, self.stop_button, self.emergency_button, self.resume_button):
            control.configure(state=state)
        self.disconnect_button.configure(state=state)
        self.connect_button.configure(state="disabled" if enabled else "normal")
        self.search_button.configure(state="disabled" if enabled else "normal")

    def _log(self, message: str):
        self.log.configure(state="normal")
        self.log.insert("end", message.strip() + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _future(self, future: concurrent.futures.Future, action: str):
        def done(result):
            try:
                self.events.put((action, result.result()))
            except Exception as exc:  # UI must show BLE and Windows pairing errors.
                self.events.put(("error", f"{action}: {exc}"))
        future.add_done_callback(done)

    def _poll_events(self):
        while True:
            try:
                action, value = self.events.get_nowait()
            except queue.Empty:
                break
            if action == "error":
                self._log(f"ERRO: {value}")
                messagebox.showerror("Exus Control", str(value))
                self._set_controls(False)
                self.connection_text.set("NÃO CONECTADO")
            elif action == "scan":
                self.devices = value
                labels = [f"{item.name}  ({item.rssi} dBm)" for item in self.devices]
                self.device_list["values"] = labels
                if labels:
                    self.device_choice.set(labels[0])
                    self.info_text.set("Selecione um protótipo e clique em Conectar.")
                    self._log(f"Encontrados: {', '.join(item.name for item in self.devices)}")
                else:
                    self.info_text.set("Nenhum Exus encontrado. Confira alimentação e Bluetooth.")
            elif action == "connect":
                info, zones = value
                self.connection_text.set("CONECTADO — pronto para teste")
                self.info_text.set(info)
                self._show_zones(zones)
                self._set_controls(True)
                self.connection_active = True
                self._log(f"Conectado. Zonas prontas: {zones}")
            elif action == "reply":
                self._log(str(value))
            elif action == "emergency":
                self._log("Emergência enviada. Confirme visualmente que todos os motores pararam.")
            elif action == "disconnect":
                self.connection_text.set("NÃO CONECTADO")
                self.info_text.set("Conexão encerrada. O firmware deve ter parado todos os motores.")
                self._set_controls(False)
                self.connection_active = False
                self._log("Desconectado. A placa deve interromper as zonas por segurança.")
            elif action == "health" and not value and self.connection_active:
                self.connection_text.set("CONEXÃO PERDIDA")
                self.info_text.set("Bluetooth desconectou. O firmware deve ter parado todos os motores.")
                self._set_controls(False)
                self.connection_active = False
                self._log("Conexão BLE perdida; controles desabilitados por segurança.")
        self.root.after(100, self._poll_events)

    def _health_check(self):
        if self.connection_active:
            self._future(self.worker.submit(self.worker.is_connected()), "health")
        self.root.after(1000, self._health_check)

    def search(self):
        self.info_text.set("Procurando protótipos BLE próximos...")
        self._future(self.worker.submit(self.worker.scan()), "scan")

    def connect(self):
        try:
            index = list(self.device_list["values"]).index(self.device_choice.get())
            device = self.devices[index]
        except (ValueError, IndexError):
            messagebox.showwarning("Exus Control", "Procure e selecione um protótipo primeiro.")
            return
        self.connection_text.set("CONECTANDO / PAREANDO...")
        self._future(self.worker.submit(self.worker.connect(device)), "connect")

    def disconnect(self):
        self._future(self.worker.submit(self.worker.disconnect()), "disconnect")

    def _show_zones(self, zones: list[int]):
        for widget in self.zones_content.winfo_children():
            widget.destroy()
        self.zone_vars = {}
        for index, zone in enumerate(zones):
            variable = tk.BooleanVar(value=False)
            self.zone_vars[zone] = variable
            ttk.Checkbutton(self.zones_content, text=f"Zona {zone}", variable=variable).grid(
                row=index // 6, column=index % 6, sticky="w", padx=(0, 16), pady=2
            )

    def _selected_mask(self) -> int:
        return sum(1 << zone for zone, variable in self.zone_vars.items() if variable.get())

    def _read_test_values(self) -> tuple[int, int, int] | None:
        try:
            intensity, duration, frequency = self.intensity.get(), self.duration.get(), self.frequency.get()
        except tk.TclError:
            messagebox.showwarning("Exus Control", "Preencha intensidade, duração e ritmo com números.")
            return None
        if not 1 <= intensity <= 50 or not 1 <= duration <= 2000 or not 1 <= frequency <= 100:
            messagebox.showwarning("Exus Control", "Valores permitidos: 1–50%, 1–2000 ms e 1–100 Hz.")
            return None
        return intensity, duration, frequency

    def test_zones(self):
        mask = self._selected_mask()
        values = self._read_test_values()
        if not mask:
            messagebox.showwarning("Exus Control", "Marque ao menos uma zona.")
            return
        if not values:
            return
        intensity, duration, frequency = values
        command = f"group 0x{mask:X} pulse {intensity} {duration} {frequency}"
        self._log(f"Enviando teste simultâneo: {command}")
        self._future(self.worker.submit(self.worker.command(command)), "reply")

    def stop_all(self):
        self._future(self.worker.submit(self.worker.command("stop all")), "reply")

    def emergency(self):
        if messagebox.askyesno("Emergência", "Enviar parada imediata para todas as zonas?"):
            self._future(self.worker.submit(self.worker.emergency()), "emergency")

    def resume(self):
        if messagebox.askyesno("Liberar emergência", "A montagem foi inspecionada e é seguro liberar a emergência?"):
            self._future(self.worker.submit(self.worker.command("resume")), "reply")

    def close(self):
        self.worker.close()
        self.root.destroy()


def main():
    root = tk.Tk()
    ExusControl(root)
    app = root.winfo_children()[0]
    root.protocol("WM_DELETE_WINDOW", app.close)
    root.mainloop()


if __name__ == "__main__":
    main()
