"""Cliente de bancada do Exus Control."""

from __future__ import annotations

import argparse
import asyncio
import sys

from .ble_client import BleakError, ExusBleClient, find, scan


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
        if args.action == "info": print(await client.info())
        elif args.command.strip().lower() == "emergency":
            await client.emergency(); print("Parada de emergência enviada. Confirme no protótipo que todos os motores pararam.")
        else: print(await client.command(args.command))
    finally:
        await client.disconnect()
    return 0


def parse_args():
    parser = argparse.ArgumentParser(description="Cliente BLE de bancada do Exus")
    sub = parser.add_subparsers(dest="subcommand", required=True)
    scan_parser = sub.add_parser("scan", help="listar protótipos Exus BLE próximos")
    scan_parser.add_argument("--seconds", type=float, default=5.0)
    connect = sub.add_parser("connect", help="conectar a um Exus")
    connect.add_argument("--id", dest="identifier", required=True)
    connect_sub = connect.add_subparsers(dest="action", required=True)
    connect_sub.add_parser("info")
    command = connect_sub.add_parser("command")
    command.add_argument("command")
    return parser.parse_args()


def main() -> None:
    try: raise SystemExit(asyncio.run(main_async(parse_args())))
    except (BleakError, RuntimeError, TimeoutError) as exc:
        print(f"Erro BLE: {exc}", file=sys.stderr); raise SystemExit(2)
