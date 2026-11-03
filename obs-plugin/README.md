# Fora de Cena — filtro para OBS Studio

Este diretório contém o plugin nativo do OBS. A integração será feita diretamente
na fonte de webcam selecionada no OBS: o plugin recebe quadros do pipeline do OBS
e não abre uma segunda captura da câmera.

## Estado atual

**Protótipo de integração OBS (0.1.0):**
- registra o filtro de vídeo "Fora de Cena";
- oferece controles iniciais de ativação e seleção de efeito;
- mantém o vídeo original intacto enquanto o processamento é integrado.

Gestos, segmentação, calibração e efeitos ainda não estão conectados. Esta não é
uma versão funcional dos efeitos no OBS.

## Arquitetura planejada

```text
Webcam física → Fonte OBS → Filtro nativo → IPC local → Worker de visão
     ↑                                                   ↓
     └──────── Cena / gravação / transmissão ← quadro processado
```

A câmera permanece aberta exclusivamente pelo OBS. O worker não pode usar
`cv2.VideoCapture`. A comunicação terá fila limitada, sequência de quadros e
timeout; em falha, o filtro preserva o quadro original sem bloquear o render.

## Compilação

Requisitos: Windows 10/11 x64, OBS Studio x64 e SDK/libobs correspondente,
CMake 3.20+ e Visual Studio 2022 com C++ Desktop.

O CMake espera que `find_package(libobs)` encontre a configuração do pacote
libobs fornecida pelo ambiente de desenvolvimento do OBS.

```powershell
cmake -S obs-plugin -B build/obs-plugin -A x64
cmake --build build/obs-plugin --config Release
```

## Licenças

O código original do aplicativo principal está sob MIT conforme a licença raiz.
O programa independente em `camera_output/` permanece GPL-2.0-only e não é
incorporado ao plugin. Bibliotecas e modelos mantêm suas licenças próprias.
