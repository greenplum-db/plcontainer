#!/bin/sh
''''which python >/dev/null 2>&1 && exec python "$0" "$@" # '''
''''which python3  >/dev/null 2>&1 && exec python3  "$0" "$@" # '''
''''exec echo "Error: I can't find python anywhere"         # '''
# python/python3 may not co-exist always on all testing images.
# Trick from https://stackoverflow.com/a/26056481/1396606

import argparse

def print_client(image):
    K = {
        'r.alpine':'r4.3.0',
        'r.ubuntu1804':'', # default
        'python27.ubuntu':'', # default
        'python39.ubuntu2204':'', # default
        'python39.alpine':'python39',
        'python39.ubuntu2204_b':'', # default
    }

    for (k, v) in K.items():
        if k.startswith(image) and v != '':
            print("-s client=" + v)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', type=str, required=True, help='the image name')

    args = parser.parse_args()

    print_client(args.i)


if __name__ == '__main__':
    main()
