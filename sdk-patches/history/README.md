# The SDK patch's history

`xboxxbe-recomp-commits.mbox` holds each commit behind `../xboxxbe-recomp.patch`, one after another, with its
message: how each ray tracing change was found and measured. The commits start from TGP482/XboxXBE-Recomp
`91146ff`, the SDK version the port builds on.

To get them back as commits:

    git clone https://github.com/TGP482/XboxXBE-Recomp.git
    cd XboxXBE-Recomp
    git checkout -b rt 91146ff
    git am path/to/xboxxbe-recomp-commits.mbox
