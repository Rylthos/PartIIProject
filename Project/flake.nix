{
  description = "Flake for cpp vulkan development";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in {
      devShells.${system}.default = pkgs.mkShell {
        nativeBuildInputs = with pkgs; [
          vulkan-headers
          vulkan-loader
          vulkan-validation-layers
          vulkan-extension-layer
          vulkan-tools
          vulkan-tools-lunarg

          glfw

          renderdoc
        ];

        buildInputs = with pkgs; [
          shader-slang
          tracy

          ninja
          cmake
          cmake-language-server
          clang-tools
          clang
        ];

        LD_LIBRARY_PATH="${pkgs.vulkan-loader}/lib:${pkgs.vulkan-validation-layers}/lib";
        VULKAN_SDK = "${pkgs.vulkan-headers}";
        VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";

        VulkanHeaders_DIR = "${pkgs.vulkan-headers}";
        glfw3_DIR = "${pkgs.glfw}/lib/cmake/glfw3";

        CMAKE_INCLUDE_PATH = "${pkgs.vulkan-headers}/include";
        CMAKE_LIBRARY_PATH = "${pkgs.vulkan-loader}/lib";
      };
    };
}
