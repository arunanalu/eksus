#!/usr/bin/env python3
"""Entrada de empacotamento do Exus Control.

Fica na raiz para que PyInstaller resolva o pacote ``exus_control`` sem a
colisão nominal com o wrapper histórico ``tools/exus_control.py``.
"""

from exus_control.app import main


if __name__ == "__main__":
    main()
