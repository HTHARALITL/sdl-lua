# SDL Lua Demo

Pequena demo em C que abre uma janela com SDL2 e desenha um retângulo (o jogador) que se move pelo teclado. A posição, o tamanho, a velocidade e a cor do jogador são lidos de um arquivo de configuração em Lua (`config.lua`), então dá para mudar tudo sem recompilar.

## Controles

- **W / A / S / D** ou **setas**: mover o jogador
- Fechar a janela: sair

## Configuração (`config.lua`)

```lua
player = {
    x = 100, y = 200,
    width = 50, height = 100,
    speed = 400.0,
    color = { r = 0, g = 10, b = 255, a = 255 },
}
```

Se o arquivo não existir ou algum campo estiver faltando, o programa usa valores padrão e avisa no terminal.

## Dependências

- SDL2
- Lua 5.4 (com cabeçalhos de desenvolvimento)
- gcc, make e pkg-config

No Arch/CachyOS:

```bash
sudo pacman -S sdl2 lua pkgconf base-devel
```

No Ubuntu/Debian:

```bash
sudo apt install libsdl2-dev liblua5.4-dev pkg-config build-essential
```

## Compilar e executar

```bash
make
./sdl-lua-demo
```

Execute o programa **dentro da pasta do projeto**, porque o `config.lua` é procurado no diretório atual.
