"""Carregamento dos perfis de resposta háptica por jogo."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Any


class ProfileError(ValueError):
    """O perfil de um jogo está ausente ou malformado."""


def load_profile(profile_id: str) -> dict[str, Any]:
    """Carrega um perfil versionado empacotado junto ao Exus Control."""
    safe_name = profile_id.replace("/", ".")
    path = Path(__file__).with_name("profiles") / f"{safe_name}.json"
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ProfileError(f"não foi possível carregar perfil {profile_id!r}: {exc}") from exc
    if data.get("profile_id", data.get("profile")) != profile_id or not isinstance(data.get("zones"), dict):
        raise ProfileError(f"perfil inválido em {path}")
    return data
