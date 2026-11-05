# The SDK patch's history

`xboxxbe-recomp-commits.mbox` holds each commit of the fork's own SDK work (ray tracing, DLSS Ray Reconstruction,
the shaders kept and prepared ahead, the texture pack's pictures let go), one after another, with its message: how
each change was found and measured. The commits start from TGP482/XboxXBE-Recomp `91146ff`, the SDK version the port
builds on, and end where the fork was merged onto 1.8.7: `../xboxxbe-recomp.patch` is 1.8.7's patch with them merged
in, and adds the Linux build, the own TAA and the rest of 1.8.7 that these commits do not have.

To get them back as commits:

    git clone https://github.com/TGP482/XboxXBE-Recomp.git
    cd XboxXBE-Recomp
    git checkout -b rt 91146ff
    git am path/to/xboxxbe-recomp-commits.mbox
