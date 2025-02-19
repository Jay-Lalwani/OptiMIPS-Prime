#!/bin/bash

# Run the container and execute commands inside
apptainer run "$CONTAINERDIR/mips-gcc-10.3.0.sif" << 'EOF'
make
make test
make clean
exit
EOF

# Now that we're out of the container, run additional commands
python3 compare.py all
