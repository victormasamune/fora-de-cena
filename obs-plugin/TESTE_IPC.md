# Testar o envio e retorno de vídeo

Esta etapa devolve os mesmos pixels, sem gestos, calibração ou efeitos. O worker
não abre webcam nem câmera virtual. O OBS continua sendo o dono da captura.
Não substitua a DLL antiga antes de compilar esta versão com sucesso.

## Atualizar preservando as duas alterações locais conhecidas

Na raiz do seu repositório no PowerShell:

```powershell
cd "$env:USERPROFILE\Documents\GitHub\fora-de-cena"
git stash push -m "Backup configuracao OBS antes do IPC" -- obs-plugin/CMakeLists.txt obs-plugin/src/plugin-main.cpp
git pull --ff-only origin feature/obs-webcam-filter
```

O stash guarda suas alterações. Elas foram incorporadas à nova versão: finders
OBS, libobs CONFIG e uso do efeito padrão do OBS. A busca direta de FFmpeg foi
removida: o plugin não usa essa API; o SDK libobs resolve suas próprias dependências. Não é preciso reaplicar
o stash por cima dos novos arquivos. Se o pull falhar, envie o erro; não descarte
outras alterações. Se esses arquivos já estiverem limpos, o stash apenas informa
que não há alterações locais para guardar.

## Compilar com a configuração existente

No mesmo terminal em que a compilação anterior funcionou:

```powershell
powershell -NoProfile -File .\tools\build_obs_plugin.ps1
```

O script procura uma única pasta de build nas posições comuns (`obs-plugin/build_x64`,
`obs-plugin/build`, `build_x64`, `build`) e reaproveita seu cache. Se não puder
escolher, passe a pasta que contém o `CMakeCache.txt` do plugin:

```powershell
powershell -NoProfile -File .\tools\build_obs_plugin.ps1 -BuildDir 'C:\caminho\do\build_x64'
```

O caminho acima é um exemplo: use o seu caminho real. O script não instala nada.
Ao terminar, ele mostra o caminho completo da DLL produzida. Feche o OBS,
guarde uma cópia da DLL antiga e substitua somente `fora-de-cena-obs.dll` na
pasta em que instalou o protótipo. Abra o OBS novamente.

### Se aparecer `FindFFmpeg.cmake` não encontrado

Uma versão anterior exigia FFmpeg diretamente no CMake do plugin, antes de
carregar o SDK libobs. Essa exigência foi removida. Na raiz do repositório,
atualize a branch e repita a compilação:

```powershell
git pull --ff-only origin feature/obs-webcam-filter
powershell -NoProfile -File .\tools\build_obs_plugin.ps1
```

Não é preciso apagar o cache, reinstalar o Python ou baixar outro FFmpeg para
corrigir essa busca. `libobs_DIR`, `CMAKE_PREFIX_PATH`, o gerador e o SDK Windows
continuam vindo do seu cache. Se surgir outro erro, envie a saída completa e o
caminho de build mostrado pelo script para identificar a dependência real do SDK.

### Se aparecer `FindSIMDe.cmake` não encontrado

O SDK libobs requer os cabeçalhos SIMD Everywhere. Alguns SDKs temporários
exportam `libobsConfig.cmake` para `.deps/cmake` sem incluir os módulos de busca.
O projeto agora inclui um módulo de busca próprio e reutiliza os caminhos do
cache, inclusive pacotes `obs-deps-*` vizinhos a esse arquivo.

Atualize e repita o mesmo comando de compilação. Nenhuma etapa de Python precisa
ser repetida. Se o novo erro disser que `simde/simde-common.h` não existe, envie
a saída: isso distingue falta do módulo de busca de falta dos cabeçalhos reais.
O build continua exigindo os arquivos do SDK; não cria uma dependência fictícia.

## Iniciar o worker e configurar o filtro

Na raiz do projeto:

```powershell
.\.venv\Scripts\python.exe -m teleport_cam.obs_worker
```

Também é possível abrir `INICIAR_WORKER_OBS.bat`. Mantenha essa janela aberta.
Ela mostra uma **porta** e uma **chave** de sessão. A chave muda a cada reinício
do worker. O comando não requer novos pacotes nem os modelos nesta etapa.

1. No OBS, selecione a fonte **Dispositivo de captura de vídeo** da webcam.
2. Abra **Filtros**. Na seção de filtros de áudio/vídeo (assíncronos), adicione
   **Fora de Cena IPC (experimental)**. O filtro antigo **Fora de Cena** continua
   sendo apenas de passagem; adicionar o novo não exige apagar o antigo.
3. Copie **Porta** e **Chave** do terminal para as propriedades do novo filtro.
4. Marque **Ativar teste IPC**. O worker deve imprimir `IPC_OK: quadro ... devolvido sem efeitos.`
5. Feche e reabra as propriedades do filtro para atualizar a leitura do contador
   **IPC conectado · quadros retornados**. O contador prova o retorno; a imagem
   deve continuar com a mesma aparência, porque não há efeitos neste teste.
6. Encerre o worker com Ctrl+C. A imagem deve continuar ao vivo. Reabra o worker,
   copie a nova porta/chave e confirme a reconexão e o retorno do contador.

Se o filtro novo não aparecer, a DLL não foi atualizada/carregada ou a fonte não
é assíncrona. Se não houver `IPC_OK`, confira porta/chave, ativação, fonte e formato.
Para testar formato, use as propriedades da fonte da webcam no OBS. Esta versão
aceita BGRA, BGRX, RGBA, NV12, I420, YUY2, UYVY e YVYU de 8 bits, até 3840×2160.
YUV 4:2:0 exige dimensões pares; YUV 4:2:2 exige largura par. Formatos HDR/10 bits
ou não suportados mantêm a imagem original sem enviar quadros.

O filtro não espera o socket: há um quadro pendente, um em trânsito e o resultado
mais recente. Respostas com mais de 250 ms são descartadas. O timestamp da captura
atual é mantido; a imagem de retorno pode corresponder a uma captura anterior.
Este é um teste de transporte, ainda sem garantia de latência/sincronia para uso
em live. Primeiro teste em uma cena local com a webcam.

## Evidência automatizada

```powershell
.\.venv\Scripts\python.exe -m pytest tests/test_obs_ipc.py -v
```

Os testes exercitam autenticação, limites, mensagens incompletas, prazos e retorno
de pixels. Com `g++` disponível, também compilam e executam o cliente C++ contra
o worker Python em loopback, nos oito formatos, inclusive fallback. Sem `g++`,
só essa parte nativa é ignorada; o plugin Windows é compilado com MSVC/CMake.

Esses testes não carregam libobs e não comprovam a compilação da DLL no Windows.
O resultado da compilação e o teste na webcam precisam ser conferidos na máquina
com OBS 33.0.1 e as dependências que você já configurou.
