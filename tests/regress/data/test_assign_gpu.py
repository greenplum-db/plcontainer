#!/bin/sh
''''which python >/dev/null 2>&1 && exec python "$0" "$@" # '''
''''which python3  >/dev/null 2>&1 && exec python3  "$0" "$@" # '''
''''exec echo "Error: I can't find python anywhere"         # '''
# python/python3 may not co-exist always on all testing images.
# Trick from https://stackoverflow.com/a/26056481/1396606

import os
import argparse
import xml.etree.ElementTree as ET

def find_runtime(xmltree, runtimeid):
    for runtime in list(xmltree):
        for e in list(runtime):
            if e.tag == 'id' and e.text == runtimeid:
                return runtime

def assign_gpu(runtimenode, action):
    if action == 'all':
        # add <device_request type="gpu" all="true" />
        gpu_node = ET.Element("device_request", attrib = {"type": "gpu", "all": "true"})
        runtimenode.append(gpu_node)
    else:
        raise RuntimeError("unknown action " + action)

def xml_prettify(elem):
    import xml.dom.minidom as md
    xml_str = ET.tostring(elem).decode()

    dom = md.parseString(xml_str)
    pretty_xml = dom.toprettyxml(indent = '    ')
    pretty_xml = os.linesep.join([s for s in pretty_xml.splitlines() if s.strip()])

    return pretty_xml

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--file',    type=str, required=True, help='the file want to process')
    parser.add_argument('-i',        action='store_true', help='edit the file in inplace')
    parser.add_argument('--runtime', type=str, required=True, help='the runtime id')
    parser.add_argument('--action',  type=str, required=True, choices=['all'], help='the action')

    args = parser.parse_args()

    xmltree = ET.parse(args.file).getroot()
    runtime = find_runtime(xmltree, args.runtime)

    if runtime is None:
        raise RuntimeError("runtimeid '" + args.runtime + "' did not find in file '" + args.file + "'")

    assign_gpu(runtime, args.action)

    out_xml = xml_prettify(xmltree)

    if args.i:
        with open(args.file, "w+") as f:
            f.write(out_xml)
    else:
        print(out_xml)

if __name__ == '__main__':
    main()
