# Segurança

Esta é uma versão de teste local para Windows. Não há certificação de segurança
ou garantia de ocultação perfeita. A validação automatizada não substitui o
teste com câmera, driver e navegador reais.

## Fronteiras que devem permanecer

- No aplicativo desktop, frames ficam no app, no processo independente de saída
  e na câmera virtual escolhida; comunicação por pipes anônimos locais. Esse
  fluxo não inicia o worker experimental do OBS.
- A saída é iniciada por caminho fixo do pacote, sem shell, comandos externos ou
  código recebido pela rede. Os canais têm formato/tamanho definidos, um quadro
  em trânsito e prazo de resposta. Um travamento encerra o filho e mantém prévia.
- O evento que permite sair do Meet exige confirmação do quadro pelo componente,
  após o envio real à câmera virtual; falha/timeout não confirma o desaparecimento.
- Extensão não recebe frames e não solicita captura de câmera ou microfone.
- Ponte HTTP somente em loopback; Host exato, Origin restrito e segredo local
  são exigidos. Não adicionar CORS permissivo ou escuta em 0.0.0.0.
- A saída automática começa desligada, exige vínculo e saída virtual ativa,
  e só começa depois de enviar o frame final do desaparecimento.
- Um comando é de uso único, tem prazo curto e identifica sala e aba. A extensão
  confere novamente o endereço antes de fechar. Não fechar outras abas/janelas.
- Restauração, parada, desvínculo ou perda de conexão cancelam a contagem.
- A atualização de dependências/modelos é explícita. Modelos são conferidos
  contra hashes fixados no código, também durante instalação. O instalador não
  deve regenerar uma linha de confiança a partir de um arquivo alterado.
- Não publicar segredos, preferências, capturas, ambientes virtuais ou arquivos
  de trabalho. O ZIP de fontes exclui modelos. O candidato binário inclui somente
  os três modelos oficiais com hashes fixados e os avisos documentados em
  THIRD_PARTY_NOTICES.md. Configurações e chave são criadas por usuário, fora da instalação.
- O instalador não deve exigir administrador, registrar drivers, instalar
  extensão, iniciar com Windows ou abrir a câmera automaticamente. O OBS é
  instalado separadamente pelo usuário. Não orientar a desativar antivírus.

O segredo não protege contra malware ou outro processo com acesso à mesma
conta do Windows. F8 pode estar ocupado por outro aplicativo; a interface
informa o fallback e mantém o botão Restaurar. Após a extensão receber um
pedido de saída, cancelar pode ser tarde demais; o aplicativo não reabre abas.

## Transporte experimental do OBS

O worker `python -m teleport_cam.obs_worker` é iniciado manualmente e recebe
quadros somente após o usuário ativar o filtro IPC. Ele escuta exclusivamente em
`127.0.0.1`, numa porta local (aleatória por padrão), e exige uma chave aleatória
nova a cada inicialização antes de ler imagens. Não inicia câmera, motor de visão,
saída virtual, interface, servidor HTTP ou comandos recebidos. O protocolo binário
não usa pickle, eval, caminhos de arquivo nem execução de comandos.

São aceitos no máximo quatro clientes simultâneos, com limites de dimensões,
comprimento e prazo total de mensagem. O plugin mantém somente um quadro pendente,
um em trânsito e o resultado mais recente; imagens vencidas ou respostas com
metadados divergentes são descartadas. Não há espera de socket no callback do OBS.
O modo inicial devolve as mesmas imagens, sem efeitos. A captura original continua
quando o worker fica indisponível. Não se deve expor esse serviço fora do loopback.

A chave fica nas propriedades do filtro e pode acompanhar uma coleção de cenas
exportada; ela expira quando o worker é encerrado. Não inclua a chave atual em
relatórios públicos. O worker não grava quadros e sua saída de diagnóstico contém
somente dimensões, porta e chave de sessão. Não há isolamento contra processos
maliciosos executados sob a mesma conta do usuário.

## Relatar um problema

Para vulnerabilidades, use **Security → Report a vulnerability** neste repositório.
Não publique tokens, imagens de reuniões ou passos exploráveis em issues públicas.
Informe versão, impacto e uma reprodução sem dados pessoais pelo canal privado.
Somente a versão mais recente será mantida inicialmente.

## Evidência desta entrega

Testes locais cobrem recusa de token/Origin/Host, expiração, uso único, vínculo,
troca de URL, cancelamento e fallback. Não houve teste de navegador ou câmera
real nesta etapa. A auditoria anterior não certifica alterações futuras.
