{ ... }:

{
  overlays = [(
    finalPkgs: prevPkgs: {
      trilinos-mpi = prevPkgs.trilinos-mpi.overrideAttrs (final: prev: rec {
        version = "14.4.0";

        buildInputs = prev.buildInputs ++ [ prevPkgs.netcdf prevPkgs.openblas ];
        
        preConfigure = prev.preConfigure + ''
          cmakeFlagsArray+=(-DTrilinos_ENABLE_ML=ON \
                            -DTPL_ENABLE_Matio=OFF \
                            -DTrilinos_ENABLE_SEACAS=ON \
                            -DTPL_ENABLE_X11=OFF
                            -DBLAS_LIBRARY_NAMES="openblas" \
                            -DBLAS_LIBRARY_DIRS=${prevPkgs.openblas}/lib \
                            -DLAPACK_LIBRARY_NAMES="openblas" \
                            -DLAPACK_LIBRARY_DIRS=${prevPkgs.openblas}/lib);
        '';


        
        src = prev.src.override {
          rev = "${prev.pname}-release-${prevPkgs.lib.replaceStrings [ "." ] [ "-" ] version}";
          sha256 = "sha256-jbXQYEyf/p9F2I/I7jP+0/6OOcH5ArFlUk6LHn453qY=";
        };
      });
    }
  )];

  config = {
    allowUnfree = true;
  };
}
