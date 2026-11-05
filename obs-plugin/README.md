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

**Protótipo 0.2.0, transporte experimental sem efeitos:**

- preserva o filtro original **Fora de Cena** e usa o efeito padrão do OBS;
- adiciona **Fora de Cena IPC (experimental)** para fontes assíncronas de webcam;
- oferece worker Python local iniciado manualmente, com autenticação por sessão;
- envia quadros em thread separada e recebe os mesmos pixels de volta;
- mantém somente o quadro pendente mais recente e descarta respostas vencidas;
- preserva o vídeo original em falha, formato não suportado ou worker desligado;
- reaproveita o SDK libobs e os caminhos do cache da configuração Windows;
- deixa o SDK carregar suas dependências, sem exigir FFmpeg diretamente no plugin;
- inclui busca de cabeçalhos SIMDe para SDKs temporários sem seus módulos CMake.

O motor Python já aceita `LatestFrameInput` sem abrir outra webcam ou câmera
virtual. Este marco testa apenas o transporte de quadros nativos: o worker IPC
ainda não chama o motor, os gestos ou os efeitos.

**Teste guiado e atualização da DLL:** [TESTE_IPC.md](TESTE_IPC.md).

## Arquitetura

A fonte OBS fornece os pixels ao callback assíncrono; o plugin copia somente as
linhas úteis, excluindo padding. Um worker C++ envia o pacote binário autenticado
via TCP em `127.0.0.1`. O Python valida dimensões, formato, sequência e tamanho,
e devolve os mesmos bytes. O próximo callback do OBS aplica o resultado recente
compatível com o formato e as dimensões atuais. Nenhum socket é lido ou escrito
no callback da captura. O worker não grava vídeo nem inicia capturas adicionais.

São aceitos formatos de 8 bits BGRA/BGRX/RGBA, NV12/I420 e YUY2/UYVY/YVYU até
3840×2160. O filtro original síncrono continua compatível com cenas já salvas.
O novo filtro IPC é específico para fontes assíncronas, como a webcam do OBS.

## Validação e próximos marcos

O núcleo C++ do transporte foi compilado com GCC e testado contra o Python em
loopback, incluindo retorno exato nos oito formatos, padding, chave inválida,
resposta inconsistente, timeout e encerramento. A suíte não usa câmera nem libobs.
A compilação da DLL MSVC e o carregamento no OBS 33 ainda precisam ser validados
no Windows. Não há certificado de latência, qualidade ou desempenho em live.

Depois do teste de ida e volta no OBS: adaptar os quadros para a entrada BGR do
motor, conectar saída processada, calibração/confirmação do fundo, restauração,
gestos e efeitos. A GUI do aplicativo desktop continua independente.

## Referências oficiais

- https://docs.obsproject.com/plugins
- https://docs.obsproject.com/graphics
- https://github.com/obsproject/obs-plugintemplate
- https://github.com/obsproject/obs-studio/releases/tag/33.0.1

## Licenças

O código original do aplicativo principal está sob MIT conforme a licença raiz.
O programa independente em `camera_output/` permanece GPL-2.0-only e não é
incorporado ao plugin. Bibliotecas e modelos mantêm suas licenças próprias.
