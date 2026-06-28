import re, sys, os

KEYWORDS = ["point","line","triangle","lineadj","triangleadj"]
def is_ident(c): return c.isalnum() or c=='_'
def is_space(c): return c in ' \t\r\n'
def is_reg_letter(c): return c in 'bcstuv'

def rewrite(src, strip_pragma_def):
    n=len(src); out=[]; i=0; saw_colon=False
    while i<n:
        # Rule A
        matched=False
        if (i==0 or not is_ident(src[i-1])):
            for kw in KEYWORDS:
                L=len(kw)
                if src[i:i+L]==kw and (i+L==n or not is_ident(src[i+L])):
                    out.append(kw+"_id"); i+=L; matched=True; break
        if matched: continue
        # Rule C then B (only on second-or-later colon)
        if src[i]==':' and saw_colon:
            # Rule C: : \s* register \s* ( \s* [bcstuv]\d+ \s* )
            m=re.match(r':\s*register\s*\(\s*[bcstuv]\d+\s*\)', src[i:])
            if m:
                out.append(' '*m.end()); i+=m.end(); saw_colon=True; continue
            m=re.match(r':\s*[bcstuv]\d+(?![A-Za-z0-9_])', src[i:])
            if m:
                out.append(' '*m.end()); i+=m.end(); saw_colon=True; continue
        # Rule E
        if strip_pragma_def and (i==0 or src[i-1]=='\n'):
            m=re.match(r'[ \t]*#[ \t]*pragma[ \t]+def(?![A-Za-z0-9_])[^\n]*', src[i:])
            if m:
                out.append(' '*m.end()); i+=m.end(); continue
        ch=src[i]
        if ch=='\n': saw_colon=False
        elif ch==':': saw_colon=True
        out.append(ch); i+=1
    return ''.join(out)

if __name__=='__main__':
    strip = (sys.argv[1]=='1')
    tree=sys.argv[2]; suffix=sys.argv[3]
    for root,_,files in os.walk(tree):
        for f in files:
            if f.endswith(('.vsh','.inc')) and suffix not in f and 'rewritten' not in f:
                p=os.path.join(root,f)
                src=open(p,'rb').read().decode('latin-1')
                rew=rewrite(src,strip)
                op=p[:-4]+suffix+p[-4:]
                open(op,'w',newline='').write(rew)
                if len(rew)!=len(src.replace('point','pointXXX')): pass
    print('done strip_pragma_def=%s'%strip)
