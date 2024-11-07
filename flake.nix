{
  inputs = {
    nixpkgs = {
      url = "github:nixos/nixpkgs/nixos-unstable";
    };
    flake-utils = {
      url = "github:numtide/flake-utils";
    };
  };
  outputs = { nixpkgs, flake-utils, ... }: flake-utils.lib.eachDefaultSystem (system:
    let
      pkgs = import nixpkgs {
        inherit system;
        config.cudaSupport = true;
        config.allowUnfree = true;
      };

      adiak = (with pkgs; stdenv.mkDerivation {
          pname = "adiak";
          version = "0.4.0";
          src = fetchgit {
            url = "https://github.com/LLNL/Adiak";
            rev = "v0.4.0";
            sha256 = "sha256-S4ZLU6f/njdZXyoQdCJIDzpQTSmfapZiRe4zIex5f0Q=";
            fetchSubmodules = true;
          };
          nativeBuildInputs = [
            cmake
          ];
          cmakeFlags = [
            "-DBUILD_SHARED_LIBS=ON"
            "-DCMAKE_BUILD_TYPE=Release"
          ];
        }
      );


      caliper = (with pkgs; stdenv.mkDerivation {
          pname = "caliper";
          version = "2.10.0";
          src = fetchgit {
            url = "https://github.com/LLNL/caliper";
            rev = "9b5b5efe9096e3f2b306fcca91ae739ae5d00716";
            sha256 = "sha256-4rnPbRYtciohLnThtonTrUBO+d8vyWvJsCgoqbJI5Rg=";
            fetchSubmodules = true;
          };
          nativeBuildInputs = [
            cmake
            adiak
            python3
          ];
          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DBUILD_SHARED_LIBS=ON"
            "-DWITH_ADIAK=ON"
            "-DWITH_LIBDW=ON"
            "-DWITH_LIBPFM=ON"
            "-DWITH_LIBUNWIND=ON"
            "-DWITH_MPI=ON"
            "-DWITH_SAMPLER=ON"
          ];
        }
      );
      
      

      # p4est-setup-src = pkgs.fetchurl {
      #   url = "https://raw.githubusercontent.com/dealii/dealii/master/doc/external-libs/p4est-setup.sh";
      #   hash = "sha256-foTrK7ga9j6dYRjkwwfct/iUOs6UOWlifsSxc/11Gh8=";
      # };

      # p4est-setup = (pkgs.writeScriptBin "p4est-setup" (builtins.readFile p4est-setup-src)).overrideAttrs(old: {
      #   buildCommand = "${old.buildCommand}\n patchShebangs $out";
      # });

      # trilinos with mpi enabled is required
      # https://github.com/NixOS/nixpkgs/blob/2eacb37fa30f518c27873b7c254d1d34859bad35/pkgs/development/libraries/science/math/trilinos/default.nix#L61
      # use this https://github.com/NixOS/nixpkgs/blob/2eacb37fa30f518c27873b7c254d1d34859bad35/pkgs/development/libraries/science/math/trilinos/default.nix#L61
      
      arborx = (with pkgs; stdenv.mkDerivation {
        pname = "arborx";
        version = "1.5";
        src = fetchgit {
          url = "https://github.com/arborx/ArborX";
          rev = "v1.5";
          sha256 = "sha256-qEC4BocPyH9mmU9Ys0nNu8s0l3HQGPHg8B1oNcGwXOQ=";
          fetchSubmodules = true;
        };
        nativeBuildInputs = [
          cmake
          openmpi
          #kokkos
          trilinos_override
          cudatoolkit
        ];
        propagatedBuildInputs = [
          cudatoolkit
        ];
        cmakeFlags = [
          "-DCMAKE_BUILD_TYPE=Release"
          "-DBUILD_SHARED_LIBS=ON"
          # "-DCMAKE_CXX_COMPILER=nvcc_wrapper"
          "-DCMAKE_CXX_EXTENSIONS=OFF"
          "-DARBORX_ENABLE_MPI=ON"
        ];
      });

      deal_II = (with pkgs; stdenv.mkDerivation {
        pname = "deal_II";
        version = "9.5.2";
        src = fetchgit {
          url = "https://github.com/dealii/dealii";
          rev = "6f07117be556bf929220c50820b4dead54dc31d0";
          sha256 = "sha256-wJIrSuEDU19eZT66MN0DIuSiWQ1/gdu+gHeMYrbQkxk=";
          fetchSubmodules = true;
        };
        nativeBuildInputs = [
          cmake
          openmpi
          #kokkos
          trilinos_override
          cudatoolkit
          arborx
          p4est
          boost183
        ];
        propagatedBuildInputs = [
          cudatoolkit
          p4est
          arborx
          trilinos_override
          boost183
        ];
        cmakeFlags = [
          "-DCMAKE_BUILD_TYPE=DebugRelease"
          # "-DCMAKE_CXX_COMPILER=nvcc_wrapper"
          "-DCMAKE_CXX_STANDARD=17"
          "-DCMAKE_CXX_EXTENSIONS=OFF"
          "-DDEAL_II_WITH_TBB=OFF"
          "-DDEAL_II_WITH_64BIT_INDICES=ON"
          "-DDEAL_II_WITH_COMPLEX_VALUES=OFF"
          "-DDEAL_II_WITH_MPI=ON"
          "-DDEAL_II_WITH_P4EST=ON"
          "-DP4EST_DIR=${p4est}"
          "-DDEAL_II_WITH_ARBORX=ON"
          "-DARBORX_DIR=${arborx}"
          "-DDEAL_II_WITH_TRILINOS=ON"
          "-DTRILINOS_DIR=${trilinos_override}"
          "-DDEAL_II_TRILINOS_WITH_SEACAS=OFF"
          "-DDEAL_II_COMPONENT_EXAMPLES=OFF"
          "-DDEAL_II_WITH_ADOLC=OFF"
          # "-DDEAL_II_WITH_BOOST=ON"
         ];
      });

      # might require enabling seacas if deal.ii requires
      trilinos_extra_args = ''
        -DTrilinos_ENABLE_ML=ON
        -DBoost_INCLUDE_DIRS=${pkgs.boost183}/include
        -DBoostLib_INCLUDE_DIRS=${pkgs.boost183}/include
        -DBoostLib_LIBRARY_DIRS=${pkgs.boost183}/lib
        -DTPL_ENABLE_BoostLib=ON 
      '';
      trilinos_withMPI = pkgs.trilinos.override (previous: { withMPI = true; boost = pkgs.boost183; });
      trilinos_override = trilinos_withMPI.overrideAttrs (previousAttrs : rec {
        preConfigure = previousAttrs.preConfigure + ''
          cmakeFlagsArray+=(${trilinos_extra_args})
        '';
        version = "14.4.0";
        src = previousAttrs.src.override {
          rev = "${previousAttrs.pname}-release-${pkgs.lib.replaceStrings [ "." ] [ "-" ] version}";
          sha256 = "sha256-jbXQYEyf/p9F2I/I7jP+0/6OOcH5ArFlUk6LHn453qY=";
        };
      });

      adamantine = (with pkgs; stdenv.mkDerivation {
        pname = "adamantine";
        version = "1.0";
        src = fetchgit {
          url = "https://github.com/adamantine-sim/adamantine";
          rev = "v1.0";
          sha256 = "sha256-pwwGgk4uIEOkyNLN26nRYvkzQZR53TJW14R9P99E3Ts=";
        };
        nativeBuildInputs = [
          deal_II
          arborx
          adiak
          caliper
          cmake
          p4est
          trilinos_override
          openmpi
          boost183
        ];
        propagatedBuildInputs = [
          deal_II
          arborx
          adiak
          caliper
          p4est
          trilinos_override
          openmpi
          boost183
        ];
        cmakeFlags = [
          "-DDEAL_II_DIR=${deal_II}"
          "-DCMAKE_BUILD_TYPE=Release"
#          "-DCMAKE_CXX_FLAGS='-g -ffast-math'"
          "-DADAMANTINE_ENABLE_ADIAK=ON"
          "-DADAMANTINE_ENABLE_CALIPER=ON"
          "-DBOOST_DIR=${boost183}"
        ];
      });
    in rec {
      defaultApp = flake-utils.lib.mkApp {
        drv = defaultPackage;
      };
      defaultPackage = adamantine;
      devShell = pkgs.mkShell {
        buildInputs = [
          pkgs.p4est
          pkgs.kokkos
          pkgs.cmake
          pkgs.openmpi
          pkgs.hdf5
          pkgs.netcdf
          trilinos_override
          adiak
          caliper
          pkgs.python3
          arborx
          pkgs.cudatoolkit
          deal_II
          adamantine
        ];
      };
    }
  );
}
