#!/usr/bin/env python3
"""Cliente de bancada BLE para o firmware Exus (SPEC-003).

Exemplos:
  python tools/exus_ble.py scan
  python tools/exus_ble.py connect --id A1B2C3 info
  python tools/exus_ble.py connect --id A1B2C3 command "pulse 0 15 500 10"
"""

from __future__ import annotations

import argparse
import asyncio
import itertools
import sys
from typing import Callable

from bleak import BleakClient, BleakScanner
from bleak.exc import BleakError

SERVICE_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10001"
COMMAND_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10002"
RESPONSE_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10003"
STATUS_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10004"
DEVICE_INFO_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10005"
EMERGENCY_UUID = "68e5d2c0-3df1-4b6a-8eb8-5ce08ad10006"


def device_name(device, advertisement_data=None) -> str:
    if advertisement_data and advertisement_data.local_name:
        return advertisement_data.local_name
    return device.name or ""


async def discover(seconds: float):
    found = await BleakScanner.discover(timeout=seconds, return_adv=True)
    rows = []
    for address, (device, advertisement) in found.items():
        name = device_name(device, advertisement)
        if name.startswith("Exus-") or SERVICE_UUID.lower() in {
            str(uuid).lower() for uuid in (advertisement.service_uuids or [])
        }:
            rows.append((address, name or "(sem nome)", advertisement.rssi))
    return sorted(rows, key=lambda row: row[1])


async def resolve(identifier: str):
    found = await BleakScanner.discover(timeout=5, return_adv=True)
    needle = identifier.lower()
    for address, (device, advertisement) in found.items():
        name = device_name(device, advertisement)
        if needle in address.lower() or needle in name.lower():
            return device
    raise RuntimeError(f"Nenhum Exus encontrado para '{identifier}'. Rode 'scan' primeiro.")


async def with_client(identifier: str, action: Callable[[BleakClient], object], pair: bool):
    device = await resolve(identifier)
    async with BleakClient(device, timeout=20.0) as client:
        if pair:
            try:
                paired = await client.pair()
                if paired is False:
                    raise BleakError("o Windows recusou o pareamento")
            except BleakError as exc:
                # No Windows, a chamada pode informar que o PC já está pareado.
                if "already" not in str(exc).lower():
                    raise
        return await action(client)


async def do_info(client: BleakClient):
    raw = await client.read_gatt_char(DEVICE_INFO_UUID)
    print(bytes(raw).decode("utf-8", errors="replace"))


async def do_command(client: BleakClient, command: str):
    if command.strip().lower() == "emergency":
        await client.write_gatt_char(EMERGENCY_UUID, b"stop", response=True)
        print("Parada de emergência enviada. Confirme no protótipo que todos os motores pararam.")
        return

    response = asyncio.get_running_loop().create_future()

    def received(_, data: bytearray):
        if not response.done():
            response.set_result(bytes(data).decode("utf-8", errors="replace"))

    await client.start_notify(RESPONSE_UUID, received)
    try:
        sequence = next(SEQUENCES)
        payload = f"@{sequence} {command.strip()}\n".encode("utf-8")
        await client.write_gatt_char(COMMAND_UUID, payload, response=True)
        print(await asyncio.wait_for(response, timeout=5.0))
    finally:
        await client.stop_notify(RESPONSE_UUID)


SEQUENCES = itertools.count(1)


async def main_async(args) -> int:
    if args.subcommand == "scan":
        rows = await discover(args.seconds)
        if not rows:
            print("Nenhum Exus encontrado. Confirme alimentação, firmware BLE e Bluetooth do PC.")
            return 1
        for address, name, rssi in rows:
            print(f"{name:16}  id/endereço: {address}  RSSI: {rssi} dBm")
        return 0

    if args.action == "info":
        await with_client(args.identifier, do_info, pair=False)
    else:
        await with_client(args.identifier, lambda client: do_command(client, args.command), pair=True)
    return 0


def parse_args():
    parser = argparse.ArgumentParser(description="Cliente BLE de bancada do Exus")
    sub = parser.add_subparsers(dest="subcommand", required=True)
    scan = sub.add_parser("scan", help="listar protótipos Exus BLE próximos")
    scan.add_argument("--seconds", type=float, default=5.0, help="duração do scan (padrão: 5)")
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
