# Projeto 1: Processamento de Imagens - Computação Visual

---

## Sumário

* [Sobre o Projeto](#sobre-o-projeto)
* [Funcionalidades](#funcionalidades)
* [Tecnologias Utilizadas](#tecnologias-utilizadas)
* [Como Compilar e Executar](#como-compilar-e-executar)
* [Estrutura do Código](#estrutura-do-código)
* [Integrantes](#integrantes)

---

## Sobre o Projeto

O objetivo do projeto é criar uma aplicação de processamento de imagens em C e SDL3. O programa recebe uma imagem, a converte para escala de cinza e permite a visualização e manipulação de seu histograma.

## Funcionalidades

- **Carregamento de Imagem**: Carrega imagens (PNG, JPG, BMP) via argumento de linha de comando e trata erros de arquivo.
- **Conversão para Escala de Cinza**: Se a imagem for colorida, converte para escala de cinza com a fórmula: $Y = 0.2125 \times R + 0.7154 \times G + 0.0721 \times B$.
- **Interface Gráfica**:
    - **Janela Principal**: Exibe a imagem, centralizada e com tamanho adaptado.
    - **Janela Secundária**: Exibe o histograma e o botão de operação.
- **Análise do Histograma**:
    - Calcula e exibe o histograma da imagem.
    - Exibe a **média de intensidade** (classificada como "clara", "média" ou "escura").
    - Exibe o **desvio padrão** (classificado como contraste "alto", "médio" ou "baixo").
- **Equalização do Histograma**:
    - Um botão permite **equalizar o histograma** e **reverter** para a imagem original em tons de cinza.
    - O texto do botão muda para refletir a ação atual.
    - O botão muda de cor para indicar os estados de mouse (neutro, hover, clique).
- **Salvar Imagem**: Pressionar a tecla **S** salva a imagem atual no arquivo `output_image.png`.


## Tecnologias Utilizadas

- **Linguagem**: C (padrão C99 ou mais recente)
- **Bibliotecas**: SDL3, SDL_image, SDL_ttf
- **Compilador**: GCC

## Como Compilar e Executar

### Pré-requisitos

É necessário ter o GCC e as bibliotecas de desenvolvimento do SDL3, SDL_image e SDL_ttf instaladas.

### Passos

1.  **Clone o repositório:**
    ```bash
    git clone [https://github.com/arthurvignati/CompVis252.git](https://github.com/arthurvignati/CompVis252.git)
    cd CompVis252
    ```

2.  **Compile o projeto:**
    ```bash
    gcc main.c -o proj1_cv -lSDL3 -lSDL3_image -lSDL3_ttf -lm
    ```

3.  **Execute o programa:**
    ```bash
    ./proj1_cv caminho/para/imagem.jpg
    ```
## Estrutura do Código

O código é organizado em funções para diferentes responsabilidades (carregamento, processamento, renderização).

- `ImageData`: Armazena os dados da imagem, incluindo um backup para a função de reverter.
- `UIContext`: Gerencia o estado da interface (janelas, histograma, botão).
- `render_loop()`: É o loop principal que processa eventos do usuário e atualiza as janelas.

## Integrantes

Projeto desenvolvido em grupo, conforme os requisitos. Nome e RA de cada integrante abaixo.

| Nome Completo     | RA        | 
| ----------------- | --------- | 
| Arthur Vignati Moscardi   |  10409688 | 
| Davi Martins Figueiredo | 10374878 | 
| Enzo Bernal de Matos| 10402685 | 
