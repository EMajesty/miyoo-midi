{
  description = "Miyoo Mini+ ARMv7 static cross-compilation environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs =
    { nixpkgs, ... }:
    let
      system = "x86_64-linux";

      pkgs = import nixpkgs {
        inherit system;
      };

      crossPkgs = import nixpkgs {
        inherit system;

        crossSystem = {
          config = "armv7l-unknown-linux-musleabihf";
        };
      };
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        packages = [
          crossPkgs.stdenv.cc
          pkgs.file
          pkgs.binutils
        ];

        shellHook = ''
          export CC="${crossPkgs.stdenv.cc.targetPrefix}cc"

          echo "Miyoo Mini+ ARMv7 static build environment"
          echo "CC=$CC"
        '';
      };
    };
}
