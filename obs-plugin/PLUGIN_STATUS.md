# Plano de implementação

- [x] Criar módulo nativo e registrar filtro de passagem.
- [x] Definir Windows 11 x64 e OBS Studio 33.x como alvo.
- [x] Separar captura e entrada externa em `teleport_cam/engine.py`.
- [x] Implementar worker de retorno de quadros nativos, sem efeitos.
- [x] Testar protocolo, autenticação, limites, sequência e prazo de resposta.
- [x] Compilar núcleo C++ e testar ida/volta contra Python nos oito formatos.
- [x] Testar descarte por validade, fallback, padding e chave inválida no núcleo.
- [ ] Validar configuração completa CMake/MSVC e DLL no Windows.
- [ ] Validar o novo filtro assíncrono com webcam no OBS 33.0.1.
- [ ] Integrar entrada BGR e saída processada do motor ao transporte.
- [ ] Conectar calibração, confirmação do fundo e restauração ao filtro.
- [ ] Integrar gestos e todos os efeitos existentes.
- [ ] Testar mudanças de formato, reconexão e encerramento no OBS real.
- [ ] Medir FPS, latência e sincronização na máquina de destino.
- [ ] Empacotar instalador e documentar instalação/remoção.

O núcleo de transporte tem teste reproduzível; a DLL ainda não foi compilada
neste ambiente. O teste Windows está em [TESTE_IPC.md](TESTE_IPC.md).
O protótipo atual devolve os mesmos pixels e ainda não aplica efeitos.
