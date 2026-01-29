{
  description = "Flake for cpp vulkan development";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
      llvm = pkgs.llvmPackages_latest;
    in {
      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          vulkan-headers
          vulkan-loader
          vulkan-validation-layers
          vulkan-extension-layer
          vulkan-tools
          vulkan-tools-lunarg

          shader-slang

          clang-tools
          llvm.clang

          cmake
          gnumake
          ninja

          protobuf_33

          zlib

          libmsquic
          openssl
        ];

        packages = with pkgs; [
          glfw

          renderdoc

          tracy
        ];

        LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
          pkgs.vulkan-loader
          pkgs.vulkan-validation-layers
        ];

        VULKAN_SDK = "${pkgs.vulkan-headers}";
        VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";

        VulkanHeaders_DIR = "${pkgs.vulkan-headers}";
        glfw3_DIR = "${pkgs.glfw}/lib/cmake/glfw3";

        shellHook = ''
          export CMAKE_C_COMPILER=clang
          export CMAKE_CXX_COMPILER=clang++
        '';
      };
    };
}
