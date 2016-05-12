git clone git@github.com:krambn/behavioral-model p4-bmv2
git clone git@github.com:krambn/p4c-bm p4c-bmv2
git clone git@github.com:krambn/switch
cd p4-bmv2
git submodule update --init --recursive
cd ../p4c-bmv2
git submodule update --init --recursive
cd ../switch
git submodule update --init --recursive

