#!/bin/sh

echo "This test requires the ipfs daemon to be running"

cid_asdf="$(echo asdf | ipfs add --pin=false -q)"
cid_bcle="$(echo bcle | ipfs add --pin=false -q)"
cid_1234="$(echo 1234 | ipfs add --pin=false -q)"

bin/http-cat 127.0.0.1 8080 ipfs/"$cid_asdf" ipfs/"$cid_bcle" ipfs/"$cid_1234" ipfs/"$cid_1234" ipfs/"$cid_bcle" ipfs/"$cid_asdf"
