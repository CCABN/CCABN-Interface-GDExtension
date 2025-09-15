# CCABN Interface GDExtension

A minimal Godot 4 GDExtension that provides a basic interface framework for CCABN projects.

## Features

- Simple logger class for testing and verification
- Basic GDExtension structure ready for extension

## Building

1. Clone the repository with submodules:
```bash
git clone --recursive https://github.com/username/CCABN-Interface-GDExtension.git
```

2. Build the project:
```bash
mkdir build
cd build
cmake ..
make
```

## Usage

Add the `ccabn_interface.gdextension` file to your Godot project's addons folder and the compiled library will be loaded automatically.

The SimpleLogger class will be available in your Godot project for basic logging functionality.