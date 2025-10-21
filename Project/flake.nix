{
  description = "Flake for cpp vulkan development";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/fea37ae1cd8f9903a30d38966ed1ced07b141316";

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

          glfw-wayland

          renderdoc
        ];

        buildInputs = with pkgs; [
          shader-slang
        ];

        LD_LIBRARY_PATH="${pkgs.vulkan-loader}/lib:${pkgs.vulkan-validation-layers}/lib";
        VULKAN_SDK = "${pkgs.vulkan-loader}";
        VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
        glfw3_DIR = "${pkgs.glfw-wayland}/lib/cmake/glfw3";
        VulkanHeaders_DIR = "${pkgs.vulkan-headers}";
      };
    };
}
