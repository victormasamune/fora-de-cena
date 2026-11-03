# Fora de Cena — filtro para OBS Studio 33

Este diretório contém o plugin nativo do OBS. A integração será feita diretamente
na fonte de webcam selecionada no OBS: o plugin recebe quadros do pipeline do OBS
e não abre uma segunda captura da câmera.

## Alvo de compatibilidade

- OBS Studio 33.x, Windows x64.
- API nativa libobs e módulos C++.
- Toolchain inicial: Visual Studio 2022 (17.x) e CMake 3.28 ou superior.
- A versão exata do SDK/libobs usada na compilação deve corresponder ao pacote
  de desenvolvimento selecionado para a instalação do OBS.

A documentação oficial 33.0.1 descreve filtros como objetos de source do libobs,
registrados por `obs_register_source`, e o pipeline de renderização de filtros
por `obs_source_process_filter_begin` / `obs_source_process_filter_end`.
O projeto deve acompanhar o modelo de build do template oficial
`obsproject/obs-plugintemplate`, que prepara as dependências libobs no CMake.

## Estado atual

**Protótipo de integração OBS (0.1.0):**
- registra o filtro de vídeo "Fora de Cena";
- oferece opção para ativar/desativar;
- mantém o vídeo original intacto enquanto o processamento é integrado.

Gestos, segmentação, calibração e efeitos ainda não estão conectados. Esta não é
uma versão funcional dos efeitos no OBS, e a compilação ainda não foi validada.

## Arquitetura planejada

```text
Webcam física → Fonte OBS → Filtro nativo → IPC local → Worker de visão
     ↑                                                   ↓
     └──────── Cena / gravação / transmissão ← quadro processado
```

A câmera permanece aberta exclusivamente pelo OBS. O worker não pode usar
`cv2.VideoCapture`. A comunicação terá fila limitada, sequência de quadros e
timeout; em falha, o filtro preserva o quadro original sem bloquear o render.

## Próximas etapas

1. Alinhar o CMake ao template oficial do OBS e fixar a dependência libobs.
2. Compilar e carregar o filtro em OBS Studio 33.x x64.
3. Criar transporte de quadros com fila limitada e fallback para imagem original.
4. Refatorar o motor Python para aceitar quadros externos, sem abrir câmera.
5. Conectar segmentação, calibração, gestos e os efeitos existentes.
6. Empacotar o plugin e testar latência, FPS, encerramento e reconexão.

## Referências oficiais

- https://docs.obsproject.com/plugins
- https://docs.obsproject.com/graphics
- https://github.com/obsproject/obs-plugintemplate

## Licenças

O código original do aplicativo principal está sob MIT conforme a licença raiz.
O programa independente em `camera_output/` permanece GPL-2.0-only e não é
incorporado ao plugin. Bibliotecas e modelos mantêm suas licenças próprias.
