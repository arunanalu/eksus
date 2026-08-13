#!/usr/bin/env python3
"""Cliente de terminal do Exus; a interface visual usa a mesma biblioteca."""

from __future__ import annotations

import argparse
import asyncio
import sys

from bleak.exc import BleakError

from exus_ble_client import ExusBleClient, find, scan


async def main_async(args) -> int:
    if args.subcommand == "scan":
        devices = await scan(args.seconds)
        if not devices:
            print("Nenhum Exus encontrado. Confirme alimentação, firmware BLE e Bluetooth do PC.")
            return 1
        for device in devices:
            print(f"{device.name:16}  id/endereço: {device.address}  RSSI: {device.rssi} dBm")
        return 0

    client = ExusBleClient(await find(args.identifier))
    try:
        await client.connect(pair=args.action != "info")
        if args.action == "info":
            print(await client.info())
        elif args.command.strip().lower() == "emergency":
            await client.emergency()
            print("Parada de emergência enviada. Confirme no protótipo que todos os motores pararam.")
        else:
            print(await client.command(args.command))
    finally:
        await client.disconnect()
    return 0


def parse_args():
    parser = argparse.ArgumentParser(description="Cliente BLE de bancada do Exus")
    sub = parser.add_subparsers(dest="subcommand", required=True)
    scan_parser = sub.add_parser("scan", help="listar protótipos Exus BLE próximos")
    scan_parser.add_argument("--seconds", type=float, default=5.0, help="duração do scan (padrão: 5)")
    connect = sub.add_parser("connect", help="conectar a um Exus")
    connect.add_argument("--id", dest="identifier", required=True, help="trecho do nome Exus-XXXXXX ou endereço mostrado em scan")
    connect_sub = connect.add_subparsers(dest="action", required=True)
    connect_sub.add_parser("info", help="ler informações públicas do dispositivo")
    command = connect_sub.add_parser("command", help="enviar um comando háptico")
    command.add_argument("command", help='por exemplo: "pulse 0 15 500 10" ou emergency')
    return parser.parse_args()


if __name__ == "__main__":
    try:
        raise SystemExit(asyncio.run(main_async(parse_args())))
    except (BleakError, RuntimeError, TimeoutError) as exc:
        print(f"Erro BLE: {exc}", file=sys.stderr)
        raise SystemExit(2)
