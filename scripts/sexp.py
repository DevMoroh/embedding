import re
TOK = re.compile(r'\s*(?:(\()|(\))|("(?:[^"\\]|\\.)*")|([^\s()"]+))')
class Sym(str): pass
def parse(text):
    stack=[[]]; pos=0; n=len(text)
    while pos<n:
        m=TOK.match(text,pos)
        if not m or m.end()==pos: break
        pos=m.end()
        if m.group(1): stack.append([])
        elif m.group(2):
            l=stack.pop(); stack[-1].append(l)
        elif m.group(3): stack[-1].append(m.group(3)[1:-1].replace('\\"','"').replace('\\\\','\\'))
        elif m.group(4): stack[-1].append(Sym(m.group(4)))
    return stack[0][0]
def q(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'
def dump(e,ind=0):
    if isinstance(e,list):
        if all(not isinstance(x,list) for x in e):
            return '\t'*ind+'('+' '.join(dump(x) for x in e)+')'
        head=[];i=0
        while i<len(e) and not isinstance(e[i],list): head.append(dump(e[i])); i+=1
        s='\t'*ind+'('+' '.join(head)+'\n'
        for x in e[i:]:
            s+=(dump(x,ind+1) if isinstance(x,list) else '\t'*(ind+1)+dump(x))+'\n'
        return s+'\t'*ind+')'
    if isinstance(e,Sym): return str(e)
    if isinstance(e,(int,float)): return fmt(e)
    return q(e)
def fmt(v):
    if isinstance(v,int): return str(v)
    s=('%.4f'%v).rstrip('0').rstrip('.')
    return '0' if s=='-0' else s
def find(e,key): return [x for x in e if isinstance(x,list) and x and x[0]==key]
def find1(e,key):
    r=find(e,key); return r[0] if r else None
