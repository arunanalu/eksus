# Exus Control

Esta é a localização oficial do Exus Control: a ponte PC ↔ jogo ↔ protótipo,
a interface gráfica, a CLI e a distribuição Windows pertencem a esta pasta.

## Executar

Na raiz do repositório:

```powershell
python -m pip install -r exus_control\requirements.txt
python exus_control_app.py
```

Para a CLI de bancada, use `python -m exus_control.cli scan`.

## Gerar o aplicativo Windows

```powershell
powershell -ExecutionPolicy Bypass -File exus_control\build_windows.ps1
```

O script gera `exus_control\dist\Exus-Control-Windows.zip`. Extraia-o inteiro e
abra `Exus-Control.exe` mantendo a pasta `_internal` ao lado; o `.exe` sozinho
não funciona, pois depende das DLLs dessa pasta.

## Testes

No Windows, com Python instalado:

```powershell
python -m pip install -r exus_control\requirements-dev.txt
python -m pytest
```

A ponte escuta apenas `127.0.0.1:4242`, inicia em modo simulado e nunca habilita
saída física sem a autorização simultânea do jogo e do operador.

Cada execução da interface grava JSON Lines em `logs/`. Para inspecionar uma
sessão sem risco de acionar hardware, use
`python -m exus_control.replay logs\<arquivo>.jsonl`; o replay sempre usa
`MockTransport` e imprime apenas `WOULD_SEND`.
