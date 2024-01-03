#!/usr/bin/env sh
''''which python >/dev/null 2>&1 && exec python "$0" "$@" # '''
''''which python3  >/dev/null 2>&1 && exec python3  "$0" "$@" # '''
''''exec echo "Error: I can't find python anywhere"         # '''

import os
import argparse
import xml.etree.ElementTree as ET

def add_backend(tree, runtime):
    backend = {'type': 'remote_docker', 'name': 'remote_docker'}

    have_backend = False
    for element in tree:
        if element.tag == 'backend' and element.attrib == backend:
            have_backend = True

    if not have_backend:
        backend_node = ET.SubElement(tree, 'backend', attrib=backend)

        address = ET.Element('address')
        address.text = '127.0.0.1'

        port = ET.Element('port')
        port.text = '2375'

        backend_node.append(address)
        backend_node.append(port)

    for element in tree:
        if element.tag == 'runtime':

            id = element.find('id').text
            if id != runtime:
                continue
            backend_node = element.find('backend')
            if backend_node is None:
                backend_node = ET.SubElement(element, 'backend')
            backend_node.clear()
            backend_node.attrib = {'name': backend['name']}

def xml_prettify(elem):
    import xml.dom.minidom as md
    xml_str = ET.tostring(elem).decode()

    dom = md.parseString(xml_str)
    pretty_xml = dom.toprettyxml(indent = '    ')
    pretty_xml = os.linesep.join([s for s in pretty_xml.splitlines() if s.strip()])

    return pretty_xml

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Add backend to runtime.')
    parser.add_argument('filename', help='plcontainer runtime file.')
    parser.add_argument('-r', dest='runtime', help='runtime name', required=True)

    args = parser.parse_args()

    tree = ET.parse(args.filename).getroot()
    add_backend(tree, args.runtime)

    with open(args.filename, "w") as f:
        f.write(xml_prettify(tree))
