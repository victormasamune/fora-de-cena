# Fora de Cena — filtro para OBS Studio 33

Este diretório contém o plugin nativo do OBS. A integração será feita diretamente
na fonte de webcam selecionada no OBS: o plugin recebe quadros do pipeline do OBS
e não abre uma segunda captura da câmera.

## Alvo de compatibilidade

- Windows 11 x64.
- OBS Studio 33.0.1 / série 33.x.
- API nativa libobs e módulos C++.
- Toolchain alinhado ao OBS 33: Visual Studio 2026 e Windows SDK 10.0.26100.
- CMake: usar a versão suportada pelo template oficial e pela toolchain instalada
  (a referência atual do template recomenda CMake 3.30.5).

**Importante:** o template oficial `obsproject/obs-plugintemplate` ainda publica
um `buildspec.json` com OBS 31.1.1. Não devemos copiar essa configuração sem
atualizar e validar as dependências para OBS 33.0.1. A versão exata de libobs e
dos pré-requisitos precisa corresponder ao alvo do plugin.

A documentação do OBS descreve filtros como objetos de source do libobs,
registrados por `obs_register_source`, e o pipeline de renderização de filtros
por `obs_source_process_filter_begin` / `obs_source_process_filter_end`.

## Estado atual

**Protótipo de integração OBS (0.1.0):**
- registra o filtro de vídeo "Fora de Cena";
- oferece opção para ativar/desativar;
- mantém o vídeo original intacto enquanto o processamento é integrado.

O motor Python já aceita `LatestFrameInput`, uma entrada BGR com apenas o quadro
mais recente. Nesse modo, não abre `cv2.VideoCapture` nem a câmera virtual.
A entrada externa e o motor têm testes automatizados em `tests/`. Isso ainda
não conecta a DLL ao Python: o transporte IPC e o retorno do vídeo estão pendentes.

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

1. Fixar e validar as dependências libobs/obs-deps compatíveis com OBS 33.0.1.
2. Integrar o CMake ao fluxo de build oficial sem reutilizar pins obsoletos do OBS 31.
3. Compilar e carregar o filtro em OBS Studio 33.x x64.
4. Criar transporte de quadros com fila limitada e fallback para imagem original.
5. Integrar a entrada externa já disponível no motor ao transporte IPC.
6. Conectar segmentação, calibração, gestos e os efeitos existentes.
7. Empacotar o plugin e testar latência, FPS, encerramento e reconexão.

## Referências oficiais

- https://docs.obsproject.com/plugins
- https://docs.obsproject.com/graphics
- https://github.com/obsproject/obs-plugintemplate
- https://github.com/obsproject/obs-studio/releases/tag/33.0.1

## Licenças

O código original do aplicativo principal está sob MIT conforme a licença raiz.
O programa independente em `camera_output/` permanece GPL-2.0-only e não é
incorporado ao plugin. Bibliotecas e modelos mantêm suas licenças próprias.
