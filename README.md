# Projeto 1: Processamento de Imagens - Computação Visual 🖼️

[![Linguagem](https://img.shields.io/badge/Linguagem-C-blue.svg)](https://en.wikipedia.org/wiki/C_(programming_language))
[![Biblioteca](https://img.shields.io/badge/Biblioteca-SDL3-brightgreen.svg)](https://www.libsdl.org/)
[![Mackenzie](https://img.shields.io/badge/Universidade-Mackenzie-red.svg)](https://www.mackenzie.br/)

[cite_start]Este é um projeto desenvolvido para a disciplina de **Computação Visual** da **Faculdade de Computação e Informática** da **Universidade Presbiteriana Mackenzie**, sob a orientação do Prof. André Kishimoto[cite: 1, 2, 3].

---

## 📜 Sumário

* [Sobre o Projeto](#-sobre-o-projeto)
* [Funcionalidades](#-funcionalidades)
* [Tecnologias Utilizadas](#-tecnologias-utilizadas)
* [Como Compilar e Executar](#-como-compilar-e-executar)
* [Estrutura do Código](#-estrutura-do-código)
* [Contribuições](#-contribuições)

---

## 📖 Sobre o Projeto

[cite_start]O objetivo deste projeto é desenvolver um software para processamento de imagens em linguagem C, utilizando a biblioteca **SDL3** para a criação da interface gráfica e manipulação de imagens[cite: 7]. O programa carrega uma imagem fornecida pelo usuário, a converte para escala de cinza, exibe seu histograma e permite a aplicação do filtro de equalização de histograma.

> [cite_start]O programa implementa funcionalidades essenciais de processamento de imagem, como conversão de espaço de cores, análise de histograma e melhoria de contraste, tudo através de uma interface gráfica simples e interativa com duas janelas[cite: 23, 30, 36].

---

## ✨ Funcionalidades

* [cite_start]**Carregamento de Imagem**: O programa é executado via linha de comando, recebendo o caminho de uma imagem como argumento[cite: 9]. [cite_start]Suporta os formatos mais comuns (**PNG, JPG, BMP**) e trata possíveis erros, como arquivo não encontrado[cite: 17, 18].
* [cite_start]**Conversão para Escala de Cinza**: Verifica se a imagem já é monocromática[cite: 20]. [cite_start]Caso seja colorida, converte-a para escala de cinza utilizando a fórmula de luminância[cite: 21]:
    $Y = 0.2125 \times R + 0.7154 \times G + 0.0721 \times B$
* [cite_start]**Interface Gráfica Dupla**[cite: 23]:
    * [cite_start]**Janela Principal**: Exibe a imagem processada, adaptando-se ao seu tamanho e iniciando centralizada na tela[cite: 24, 25].
    * [cite_start]**Janela Secundária**: Mostra o histograma da imagem e informações analíticas, além do botão de operação[cite: 26, 27].
* [cite_start]**Análise e Exibição do Histograma**[cite: 28]:
    * [cite_start]Calcula e exibe graficamente o histograma de intensidade da imagem em escala de cinza[cite: 29].
    * [cite_start]Classifica a imagem com base na **média de intensidade** ("clara", "média" ou "escura")[cite: 31].
    * [cite_start]Classifica o contraste com base no **desvio padrão** ("alto", "médio" ou "baixo")[cite: 32].
* **Equalização do Histograma**:
    * [cite_start]Um botão na janela secundária permite **equalizar o histograma** da imagem para melhorar o contraste[cite: 35, 36].
    * [cite_start]Ao clicar novamente, a imagem **reverte para a versão original** em escala de cinza, sem a necessidade de recarregar o arquivo[cite: 37].
    * [cite_start]O texto do botão alterna para refletir a ação atual (ex: "Equalizar" / "Original")[cite: 38].
    * [cite_start]O botão possui feedback visual para os estados: neutro, mouse sobre e clicado[cite: 39].
* [cite_start]**Salvar Imagem**: Ao pressionar a tecla **S**, a imagem atualmente exibida na janela principal é salva como `output_image.png`, sobrescrevendo o arquivo se ele já existir[cite: 41, 42].

---

## 🛠️ Tecnologias Utilizadas

O projeto foi desenvolvido utilizando as seguintes tecnologias:

* [cite_start]**Linguagem**: C (padrão C99 ou mais recente) [cite: 44]
* **Bibliotecas**:
    * [cite_start]**SDL3** [cite: 7]
    * [cite_start]**SDL_image** [cite: 17, 44]
    * [cite_start]**SDL_ttf** [cite: 33]
* [cite_start]**Compilador**: GCC [cite: 45]

---
