# Generates a bunch of strings given an HTML file
# So they can be used to create a templated page

import re, sys, random, string

def echar(s):
    return s.replace("\n", "\\n").replace("\"", "\\\"")

def process(markers, inp):
    matches = re.finditer(r"({%s:([^}]+)}(.*?){/%s})" % (markers[0], markers[0]), inp, re.DOTALL)

    excl_ranges, sections = [], []
    for matchNum, match in enumerate(matches):
        section_start = match.start(1)
        section_end = match.end(1)

        excl_ranges.append((section_start, section_end))
        sections.append((match.group(2), match.group(3)))

    if len(sections) == 0:
        markers = markers[1:]
        if markers:
            return process(markers, inp)
        else:
            return [{"name": "", "content": inp, "subsections": []}]

    # Get page ranges
    pos = 0
    incl_ranges = []
    for r in excl_ranges:
        incl_ranges.append((pos, r[0]))
        pos = r[1]

    incl_ranges.append((pos, len(inp)))

    # Output a proper C file
    N = len(incl_ranges) + len(sections)
    ret = []
    for i in range(N):
        if i % 2 == 0:
            ret.append({
                "name": "",
                "content": "",
                "subsections": process(markers, inp[incl_ranges[i/2][0]: incl_ranges[i/2][1]]),
            })
        else:
            ret.append({
                "name": markers[0].lower() + ":" + sections[i/2][0],
                "content": "",
                "subsections": process(markers, sections[i/2][1]),
            })

    return ret

def serialize(p, depth=0):
    subs = []
    import pprint
    for ss in p["subsections"]:
        subs.append(serialize(ss, depth+4))

    lead = " ".join([""] * depth)

    ret = lead + '{"%s", "%s", "%s", {\n' % (echar(p["name"].split(":")[0]), echar(p["name"].split(":")[-1]), echar(p["content"]))
    for ss in subs:
        ret += lead + '%s,\n' % ss
    ret += lead + "},\n"
    ret += lead + "}\n"
    return ret

# Read page
page = open(sys.argv[1], "rb").read()

# Expand any inclusions!
# TODO

# Process the following tags
out = process(["SECTION", "LIST"], page)

hfile  = "#include <vector>\n"
hfile += "#include <string>\n"
hfile += "struct p_entry {\n"
hfile += "  std::string type, name;\n"
hfile += "  std::string content;\n"
hfile += "  std::vector<p_entry> subs;\n"
hfile += "};\n\n"
hfile += "extern const std::vector<p_entry> page_html;"

cfile  = "#include \"page.h\"\n"
cfile += "const std::vector<p_entry> page_html = {\n"
cfile += "\n,".join((serialize(x, 4) for x in out))
cfile += "};"

open("page.cc", "wb").write(cfile)
open("page.h", "wb").write(hfile)

