# Exus Control

Módulo principal da ponte PC ↔ jogo ↔ protótipo. Os arquivos em `tools/` são
entradas de compatibilidade para a CLI e o executável existentes.

## Testes

No Windows, com Python instalado:

```powershell
python -m pip install -r tools\requirements-dev.txt
python -m pytest
```

A ponte escuta apenas `127.0.0.1:4242`, inicia em modo simulado e nunca habilita
saída física sem a autorização simultânea do jogo e do operador.

Cada execução da interface grava JSON Lines em `logs/`. Para inspecionar uma
sessão sem risco de acionar hardware, use `python -m exus_control.replay logs\<arquivo>.jsonl`;
o replay sempre usa `MockTransport` e imprime apenas `WOULD_SEND`.
