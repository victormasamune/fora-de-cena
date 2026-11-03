# Plano de implementação

- [x] Criar o módulo nativo e registrar um filtro de vídeo no OBS.
- [x] Criar propriedade inicial para ativar/desativar o filtro.
- [x] Definir Windows 11 x64 e OBS Studio 33.x como alvo.
- [ ] Fixar dependências libobs/obs-deps compatíveis com OBS 33.0.1.
- [ ] Integrar o CMake ao fluxo oficial de build sem usar pins obsoletos do OBS 31.
- [ ] Validar compilação e carregamento em OBS Studio x64.
- [ ] Implementar transporte de quadros OBS → worker com fila limitada.
- [ ] Separar o motor de processamento da captura em `teleport_cam/engine.py`.
- [ ] Adaptar Detector e Segmenter aos timestamps monotônicos do fluxo OBS.
- [ ] Implementar calibração e confirmação do fundo nos controles do filtro.
- [ ] Retornar quadro processado ao filtro com sincronização e fallback seguro.
- [ ] Integrar gestos e todos os efeitos existentes.
- [ ] Empacotar instalador e documentar instalação/remoção.
- [ ] Testar latência, FPS, encerramento do OBS e reconexão do worker.

Etapas só serão marcadas como concluídas após compilação ou teste reproduzível.
O protótipo atual ainda não aplica efeitos à imagem.
