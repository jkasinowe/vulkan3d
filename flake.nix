{
  description = "Vulkan development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          cmake
          pkg-config
          bear
          clang-tools
          clang
        ];
        buildInputs = with pkgs; [
          emacs
          vulkan-volk                    
          vulkan-loader
          vulkan-headers
          vulkan-tools
          vulkan-utility-libraries
          vulkan-validation-layers
          shaderc
          glslang
          cglm
          sdl3
        ];

        shellHook = ''
        '';
      };
    };
}
