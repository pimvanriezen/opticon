const ST_WAITSTART = 0; // Waiting for varname, or '('
const ST_READVAR = 1; // Reading varname
const ST_WAITCMP = 2; // Waiting for comparator
const ST_READCMP = 3; 
const ST_WAITVAL = 4;
const ST_READVAL_QUOTED = 5;
const ST_READVAL_UNQUOTED = 6;
const ST_PUSH = 7;
const ST_WAITNEXT = 8;
const TP_EMPTY = "empty"
const TP_CMP_NUMBER = "cmp_number";
const TP_CMP_STR = "cmp_str";
const TP_ANDOR = "and_or";
const TP_NESTSTART = "(";
const TP_NESTEND = ")";
const TP_NEST = "nest";

function isspace(c) {
    if (c == " ") return true;
    if (c == "\t") return true;
    if (c == "\n") return true;
}

function isvalidvar(c) {
    return String(c).match (/[._a-zA-Z0-9]+/) ? true : false;
}

function iscmp(cc) {
    if (cc[0] == '=') return true;
    if (cc == '!=') return true;
    if (cc == '~=') return true;
    if (cc == '@<') return true; // version compare
    if (cc == '@>') return true;
    if (cc[0] == '<') return true;
    if (cc[0] == '>') return true;
}

function isnumber(c) {
    return String(c).match (/[.0-9]+/) ? true : false;
}

function querysplit(instring) {
    
    let res = [];
    let a = { type: TP_EMPTY, cmp: "", left: "", right: "" }
    let st = ST_WAITSTART;
    let nestlevel = 0;
    let rdpos = 0;
    while (instring[rdpos]) {
        let c = instring[rdpos];
        let cc = c + instring[rdpos+1];
        switch (st) {
            case ST_WAITSTART:
                if (isspace (c)) {
                    rdpos++;
                    break;
                }
                if (c == '(') {
                    res.push ({ type: TP_NESTSTART });
                    rdpos++;
                    nestlevel++;
                    break;
                }
                if (isvalidvar (c)) {
                    st = ST_READVAR;
                    a.left = c;
                    rdpos++;
                    break;
                }
                throw new Error ("Illegal character at pos "+rdpos);
                
            case ST_READVAR:
                if (isvalidvar (c)) {
                    a.left += c;
                    rdpos++;
                    break;
                }
                a.cmp = "";
                st = ST_WAITCMP;
                break;
            
            case ST_WAITCMP:
                if (isspace (c)) {
                    rdpos++;
                    break;
                }
                if (iscmp (cc)) {
                    a.cmp = c;
                    rdpos++;
                    st = ST_READCMP;
                    break;
                }
                
                throw new Error ("Illegal comparator at pos "+rdpos);
            
            case ST_READCMP:
                if (iscmp (c)) {
                    a.cmp += c;
                    rdpos++;
                    break;
                }
                
                st = ST_WAITVAL;
                break;
            
            case ST_WAITVAL:
                if (isspace (c)) {
                    rdpos++;
                    break;
                }
                if (c == '"') {
                    rdpos++;
                    st = ST_READVAL_QUOTED;
                    a.type = TP_CMP_STR;
                    break;
                }
                if (isnumber (c)) {
                    rdpos++;
                    a.right = c;
                    a.type = TP_CMP_NUMBER;
                    st = ST_READVAL_UNQUOTED;
                    break;
                }
                
                throw new Error ("Illegal value character at pos "+rdpos);
            
            case ST_READVAL_QUOTED:
                if (c == '"') {
                    rdpos++;
                    st = ST_PUSH;
                    break;
                }
                if (c == '\\') {
                    rdpos++;
                    c = instring[rdpos];
                }
                a.right += c;
                rdpos++;
                break;
            
            case ST_READVAL_UNQUOTED:
                if (isnumber (c)) {
                    rdpos++;
                    a.right += c;
                    break;
                }
                st = ST_PUSH;
                break;
            
            case ST_PUSH:
                if (a.type == TP_CMP_NUMBER) {
                    // versions can be unquoted but aren't floats
                    if (a.cmp != '@<' && a.cmp != '@>') {
                        a.right = parseFloat(a.right);
                    }
                }
                res.push (a);
                delete a;
                a = { type: TP_EMPTY, cmp: "", left: "", right: "" };
                st = ST_WAITNEXT;
                break;
            
            case ST_WAITNEXT:
                if (c == ')') {
                    if (nestlevel < 1) {
                        throw new Error ("Unmatched closing bracket");
                    }
                    res.push ({ type: TP_NESTEND });
                    nestlevel--;
                    rdpos++;
                    break;
                }
                if (cc == '&&') {
                    res.push ({ type: TP_ANDOR, cmp:"and" });
                    rdpos += 2;
                    st = ST_WAITSTART;
                    break;
                }
                if (c == '&') {
                    res.push ({ type: TP_ANDOR, cmp:"and" });
                    rdpos += 1;
                    st = ST_WAITSTART;
                    break;
                }
                if (cc == '||') {
                    res.push ({ type: TP_ANDOR, cmp:"or" });
                    rdpos += 2;
                    st = ST_WAITSTART;
                    break;
                }
                if (c == '|') {
                    res.push ({ type: TP_ANDOR, cmp:"or" });
                    rdpos += 1;
                    st = ST_WAITSTART;
                    break;
                }
                if (isspace(c)) {
                    rdpos++;
                    break;
                }
                throw new Error ("Illegal character '"+c+
                                 "' at end of statement");
            
            default:
                throw new Error ("Unhandled internal state");
        }
    }
    if (st == ST_READVAL_UNQUOTED || st == ST_PUSH) {
        if (a.cmp != '@<' && a.cmp != '@>') {
            a.right = parseFloat(a.right);
        }
        res.push (a);
    }
    else if (a.type != TP_EMPTY) {
        console.log ("Leftover:",a);
        console.log ("State:",st);
    }
    
    return res;
}

function querytree(indata) {
    let res = {
        tree:[]
    };
    
    querytree_into (indata, res.tree);
    
    res.match = function (obj) {
        return querytree_match (res.tree, obj);
    }

    return res;
}

function mkquery(q) {
    let res = null
    try {
        res = querytree(querysplit(q+' '));
    }
    catch (e) {
        console.log ("Invalid query: "+e);
        return null;
    }
    return res;
}

function vcompare (left, right) {
    let l = String(left).replace('.','-');
    let r = String(right).replace('.','-');
    let ls = l.split('.');
    let rs = r.split('.');
    
    for (let idx in rs) {
        if (rs[idx]<ls[idx]) return -1;
        if (rs[idx]>ls[idx]) return 1;
    }
    return 0;
}

function querytree_match(tree, obj) {
    for (let idx=0; idx<tree.length; ++idx) {
        let node = tree[idx];
        if (! node) continue;
        
        if (node.type == TP_ANDOR) {
            let leftres = querytree_match([node.children[0]], obj);
            let rightres = querytree_match([node.children[1]], obj);
            if (node.cmp == 'and') return (leftres && rightres);
            if (node.cmp == 'or') return (leftres || rightres);
            return false;
        }
        
        let oidx = String(node.left).replace(/[.]/g,'/');
        let left = obj[oidx];
        
        if (node.type == TP_CMP_STR) {
            switch (node.cmp) {
                case '=':
                    return left == node.right;
                
                case '!=':
                    return left != node.right;
                
                case '~=':
                    let lcl = String(left).toLowerCase();
                    let lcr = String(node.right).toLowerCase();
                    return (lcl.indexOf(lcr)>=0);
                
                default:
                    return false;
            }
        }
        
        if (node.type == TP_CMP_NUMBER) {
            switch (node.cmp) {
                case '=': return left == node.right;
                case '!=': return left !=  node.right;
                case '<': return left < node.right;
                case '<=': return left <= node.right;
                case '>': return left > node.right;
                case '>=': return left >= node.right;
                case '@<': return vcompare(left,node.right)>0;
                case '@>': return vcompare(left,node.right)<0;
                default: return false;
            }
        }
        
        console.log ("Unhandled:",node);                
    }
}

function querytree_into(indata, into, pos, oneshot) {
    if (! pos) {
        pos = 0;
    }
    
    while (indata[pos]) {
        if (indata[pos].type == TP_NESTEND) return pos+1;
        if (indata[pos].type == TP_NESTSTART) {
            let nw = { type:TP_NEST, children:[] };
            pos = querytree_into (indata, nw, pos+1);
            into.push (nw);
            return pos;
        }
        if (indata[pos].type == TP_ANDOR) {
            if (into.length < 1) {
                console.log (into);
                throw new Error ("Logical operator missing left side");
            }
            let left = into.splice (-1,1)[0];
            let out = { type:TP_ANDOR, cmp:indata[pos].cmp, children:[left] }
            into.push (out);
            pos = querytree_into (indata, out.children, pos+1);
            return pos;
        }
        into.push (indata[pos++]);
    }
    return pos;
}
