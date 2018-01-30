#include "mpc.h"
#ifdef _WIN32

print("window's shit");

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

#define LASSERT(args, cond, fmt, ...) \
	if(!(cond)){ \
	lval* err = lval_err(fmt, ##__VA_ARGS__); \
	lval_del(args); \
	return err; \
	}
  
#define INCORRECT_ARGS(arg,number) \
	return arg->count != number;

#define EMPTY_LIST(arg)	\
	return arg->count == 0;



int node_tag(mpc_ast_t* t){
	return strstr(t->tag, "expr")!=0;
}

int node_content(mpc_ast_t* t){
	return strcmp(t->contents,"(") == 0 || strcmp(t->contents,")") == 0;
}

int number_of_node(mpc_ast_t* t){

	if(t-> children_num == 0){return 1;}
	if(t-> children_num > 0){
		int res = 1;
		for(int i = 0 ; i < t->children_num ; i++){
			res = res + number_of_node(t-> children[i]);	
		}
		return res;
	}
	return 0;
}

int number_of_leave(mpc_ast_t* t){
	if(t->children_num == 0){return 1;}
	else {
		int res = 0;
		for(int i = 0; i < t->children_num; i++){
			res = res + number_of_leave( t->children[i]);
			}
		return res -1 ;	
		}
}

int number_of_branches(mpc_ast_t* t){
	if(t->children_num == 0){return 0;}
	else {
		int res = 1;
		for(int i = 0; i < t->children_num; i++){
			res = res + number_of_leave( t->children[i]);
			}
		return res;	
		}
}

struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;


typedef lval* (*lbuiltin)(lenv*, lval*);

enum LVALUE { LVAL_NUM, LVAL_ERR, LVAL_SYM, LVAL_SEXPR, LVAL_QEXPR, LVAL_FUN };

char* ltype_name(int t) {
	switch(t){
		case LVAL_ERR: return "Error";
		case LVAL_FUN: return "function";
		case LVAL_NUM: return "Number";
		case LVAL_SYM: return "Symbol";
		case LVAL_SEXPR: return "S-Exp";
		case LVAL_QEXPR: return "Q-Exp";
		default: return "Unknow";
	}
}	

struct lval{
	int type;
	
	long num;
	char* err;
	char* sym;
	lbuiltin fun; 
	
	int count;
	struct lval** cell;
};


lval* lval_num(long x){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

lval* lval_err(char* x, ...){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	
	va_list va;
	va_start(va, x);
	
	v->err = malloc(512);
	vsnprintf(v->err, 511, x, va);
	
	v->err = realloc(v->err, strlen(v->err) + 1);
	va_end(va);
	
	return v;
}

lval* lval_sym(char* x){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(x) + 1);
	strcpy(v->sym,x);
	return v;
}

lval* lval_sexpr(void){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_qexpr(void){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_fun(lbuiltin function){
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->fun = function;
	return v;
}

void lval_del(lval* v){

	switch(v->type){
		case LVAL_NUM: break;
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;
		case LVAL_FUN: break;
		case LVAL_QEXPR:
		case LVAL_SEXPR: 
			for(int i = 0; i < v->count; i++){
				lval_del(v->cell[i]);
			}
			free(v->cell);
			break;
	}
	free(v);
}

lval* lval_copy(lval* v){
	lval* x = malloc(sizeof(lval));
	x->type = v->type;
	
	switch(v->type){
		case LVAL_NUM: x->num = v->num; break;
		case LVAL_FUN: x->fun = v->fun; break;
		case LVAL_ERR:
			x->err = malloc(strlen(v->err)+1);
			strcpy(x->err,v->err); break;
		case LVAL_SYM:
			x->sym = malloc(strlen(v->sym)+1);
			strcpy(x->sym,v->sym); break;
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			x->count = v->count;
			x->cell = malloc(sizeof(lval*)*x->count);
			for(int i = 0; i < x->count; i++){
				x->cell[i] = lval_copy(v->cell[i]);
			}
			break ;
	}
	return x;
}

struct lenv{
	int count;
	char** syms;
	lval** vals;
};

lenv* lenv_new(void){
	lenv* e = malloc(sizeof(lenv));
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

void lenv_del(lenv* e){
	for(int i = 0; i < e->count; i++){
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

lval* lenv_get(lenv* e, lval* x){
	for(int i = 0; i<e->count; i++){
		if(strcmp(x->sym,e->syms[i])==0){
			return lval_copy(e->vals[i]);
		}
	}
	return lval_err("unbound symbol: %s", x->sym );
}

void lenv_put(lenv* e, lval* k, lval* v){
	for(int i=0 ; i<e->count; i++){
		if(strcmp(e->syms[i],k->sym)==0){
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}
	
	e->count++;
	e->vals = realloc(e->vals, sizeof(lval*)*e->count);
	e->syms = realloc(e->syms, sizeof(char*)*e->count);
	
	e->vals[e->count - 1] = lval_copy(v);
	e->syms[e->count - 1] = malloc(strlen(k->sym)+1);
	strcpy(e->syms[e->count - 1], k->sym);
}

lval* lval_read_num(mpc_ast_t* t){
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno == ERANGE ? 
	lval_err("invalid number %s", t->contents) : lval_num(x);  
}

lval* lval_add(lval* v, lval* x){
	v->count++ ;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count - 1] = x;
	return v;
}

lval* lval_read(mpc_ast_t* t){
	if (strstr(t->tag,"number")){return lval_read_num(t);}
	if (strstr(t->tag,"symbol")){return lval_sym(t->contents);}
	
	lval* x = NULL;
	if (strcmp(t->tag, ">") == 0) { x = lval_sexpr();}
	if (strstr(t->tag, "sexpr"))  { x = lval_sexpr();}
	if (strstr(t->tag, "qexpr"))  { x = lval_qexpr();}	
	
	for(int i=0; i < t->children_num; i++){
		if(strcmp(t->children[i]->contents, "(") == 0){ continue;}
		if(strcmp(t->children[i]->contents, ")") == 0){ continue;}
		if(strcmp(t->children[i]->contents, "{") == 0){ continue;}
		if(strcmp(t->children[i]->contents, "}") == 0){ continue;}
		if(strcmp(t->children[i]->tag, "regex") == 0){ continue;}
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}

void lval_expr_print(lval*v, char open, char close);

void lval_print(lval* v){
	switch(v->type){
	case LVAL_NUM: printf("%li", v->num); break;
	case LVAL_ERR: printf("Error: %s", v->err); break;
	case LVAL_SYM: printf("%s", v->sym);break;
	case LVAL_FUN: printf("<function>"); break;
	case LVAL_SEXPR: lval_expr_print(v,'(',')'); break;
	case LVAL_QEXPR: lval_expr_print(v,'{','}'); break;
	}
} 

void lval_expr_print(lval*v, char open, char close){
	putchar(open);
	for(int i = 0; i < v->count; i++){
		lval_print(v->cell[i]);
		if(i!= (v->count-1)){
			putchar(' ');
		}
	}
	putchar(close);
}



void lval_println(lval* v){
	lval_print(v);
	putchar('\n');
}

lval* lval_pop(lval* v, int i){
	lval* x = v->cell[i];
	memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*)*(v->count-i-1));
	v->count--;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	return x;
}

lval* lval_take(lval* v, int i){
	lval* f = lval_pop(v,i);
	lval_del(v);
	return f;
}

lval* builtin_op(lenv* e, lval* a, char* op){
	for(int i = 0; i<a->count; i++){
		if(a->cell[i]->type != LVAL_NUM){
			lval_del(a);
			return lval_err("Cannot operate on non-number. Got %s, Expected %s.", 
			ltype_name(a->cell[i]->type), ltype_name(LVAL_NUM));
		}
	}
	
	lval* x = lval_pop(a,0);
	
	if((a->count == 0) && (strcmp(op,"-")==0)){
		x->num = - x->num;
	}
	
	while(a->count > 0){
		lval* y = lval_pop(a,0);
		
		if(strcmp(op, "+") == 0){x->num += y->num;}
		if(strcmp(op, "-") == 0){x->num -= y->num;}
		if(strcmp(op, "*") == 0){x->num *= y->num;}
		if(strcmp(op, "/") == 0){
			if(y->num== 0){
				lval_del(x);lval_del(y);
				x = lval_err("Division By Zero!");
				break;
			}
			x->num /= y->num;
		}
		lval_del(y);
	}
	lval_del(a);
	return x;
}

lval* builtin_add(lenv* e, lval* a){
	return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
  return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
  return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
  return builtin_op(e, a, "/");
}

lval* builtin_head(lenv* e, lval* a){
	LASSERT(a, a->count == 1,"Function 'head' passed too many arg." " Got %i. Expected %i", a->count, 1);
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'head' passed incorrect type""Got %s. Expected %s", a->cell[0]->type , ltype_name(LVAL_QEXPR));
	LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}");
	
	lval* x = lval_take(a,0);
	while(x->count > 1){
		lval_del(lval_pop(x,1));
	}
	return x;
}

lval* builtin_tail(lenv* e, lval* a){
	LASSERT(a, a->count == 1,"Function 'tail' passed too many arg." " Got %i. Expected %i", a->count, 1);
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'tail' passed incorrect type""Got %s. Expected %s", a->cell[0]->type , ltype_name(LVAL_QEXPR));
	LASSERT(a, a->cell[0]->count != 0, "Function 'tail' passed {}");
	
	lval* x = lval_take(a,0);
	lval_del(lval_pop(x,0));
	return x;
}

lval* builtin_list(lenv* e, lval* a){
	a->type = LVAL_QEXPR;
	return a;
}

lval* lval_eval(lenv* e, lval* v);

lval* builtin_eval(lenv* e, lval* a){
	LASSERT(a, a->count == 1,"Function 'eval' passed too many arg." " Got %i. Expected %i", a->count, 1);
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'eval' passed incorrect type""Got %s. Expected %s", a->cell[0]->type , ltype_name(LVAL_QEXPR));
	
	lval* x = lval_take(a,0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y){
	while(y->count){
		x = lval_add(x,lval_pop(y,0));
	}
	lval_del(y);
	return x;
}

lval* builtin_join(lenv* e, lval* a){
	for(int i = 0; i < a->count; i++){
		LASSERT(a,a->cell[i]->type == LVAL_QEXPR, "Function 'join' passed incorrect type"" Got %s. Expected %s", a->cell[i]->type , ltype_name(LVAL_QEXPR) );
	}
	
	lval* x = lval_pop(a,0);
	while(a->count){
		x = lval_join(x,lval_pop(a,0));
	}
	lval_del(a);
	return x;
}

lval* builtin_len(lenv* e, lval* a){
	LASSERT(a, a->count == 1,"Function 'len' passed too many arg");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'len' passed incorrect type");
	
	lval* x = lval_num(lval_pop(a,0)->count);
	lval_del(a);
	return x;
}

lval* builtin_init(lenv* e, lval* a){
	LASSERT(a, a->count == 1,"Function 'init' passed too many arg");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'init' passed incorrect type");
	
	lval* x = lval_pop(a,0);
	lval* y = lval_pop(x, x->count - 1);
	
	lval_del(y);
	lval_del(a);
	return x;
}

lval* builtin_def(lenv* e, lval* a){
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR, "Function 'def' passed incorrect type. Got %s, expected %s", a->cell[0]->type, ltype_name(LVAL_QEXPR));
	lval* syms = a->cell[0];
	
	for(int i = 0; i < syms->count; i++){
		LASSERT(a, syms->cell[i]->type == LVAL_SYM, "Function 'def' cannot define non-symbol. Got %s, expected %s", a->cell[i]->type, ltype_name(LVAL_SYM));
	}
	LASSERT(a, syms->count == a->count -1, "number of values to symbols. Number of symbols: %i. Number of values: %i", syms->count, a->count -1);
	
	for(int i =0; i < syms->count ; i++){
		lenv_put(e, syms->cell[i], a->cell[i+1]);
	}
	
	lval_del(a);
	return lval_sexpr();
}

void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
	lval* k = lval_sym(name);
	lval* v = lval_fun(func);
	lenv_put(e, k, v);
	lval_del(k);lval_del(v);
}

void lenv_add_builtins(lenv* e) {
  /* List Functions */
  lenv_add_builtin(e, "list", builtin_list);
  lenv_add_builtin(e, "head", builtin_head);
  lenv_add_builtin(e, "tail", builtin_tail);
  lenv_add_builtin(e, "eval", builtin_eval);
  lenv_add_builtin(e, "join", builtin_join);
  lenv_add_builtin(e, "def",  builtin_def);
  lenv_add_builtin(e, "init", builtin_init);
  lenv_add_builtin(e, "leng", builtin_len);
  
  /* Mathematical Functions */
  lenv_add_builtin(e, "+", builtin_add);
  lenv_add_builtin(e, "-", builtin_sub);
  lenv_add_builtin(e, "*", builtin_mul);
  lenv_add_builtin(e, "/", builtin_div);
}

lval* lval_eval_sexpr(lenv* e, lval* v){
	for(int i = 0; i< v->count; i++){
		v->cell[i] = lval_eval(e, v->cell[i]);
	}
	
	for(int i = 0; i< v->count; i++){
		if(v->cell[i]->type == LVAL_ERR){return lval_take(v, i);}
	}
	if(v->count == 0){return v;}
	if(v->count == 1){return lval_take(v,0);}
	
	lval* f = lval_pop(v,0);
	if(f->type != LVAL_FUN){
		lval_del(f); lval_del(v);
		return lval_err("first element isn't a function." " Got %s. expected %s",
		ltype_name(f->type), ltype_name(LVAL_FUN));
	}
	lval* result = f->fun(e,v);
	lval_del(f);
	return result;
}

lval* lval_eval(lenv* e, lval* v){
	if(v->type == LVAL_SYM){
		lval* x = lenv_get(e,v);
		lval_del(v);
		return x;
	}
	if(v->type == LVAL_SEXPR){return lval_eval_sexpr(e, v);}
	return v;
}


int main(int argc, char** argv) {
  
  /* Create Some Parsers */
  mpc_parser_t* Number   = mpc_new("number");
  mpc_parser_t* Symbol 	 = mpc_new("symbol");
  mpc_parser_t* Sexpr	 = mpc_new("sexpr");
  mpc_parser_t* Qexpr	 = mpc_new("qexpr");
  mpc_parser_t* Expr     = mpc_new("expr");
  mpc_parser_t* Lispy    = mpc_new("lispy");
  
  /* Define them with the following Language */
  mpca_lang(MPCA_LANG_DEFAULT,
    "                                                      \
      number   : /-?[0-9]+/ ; 			                   \
      symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/  ;	   \
      sexpr	   : '(' <expr>* ')' ;						   \
      qexpr	   : '{' <expr>* '}' ;						   \
      expr     : <number> | <symbol> | <sexpr> | <qexpr>;  \
      lispy    : /^/  <expr>* /$/ ;			               \
    ",
    Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  puts("Lispy Version 0.0.0.0.2");
  puts("Press Ctrl+c to Exit\n");
  
  lenv* e = lenv_new();
  lenv_add_builtins(e);
  
  while (1) {
  
    char* input = readline("lispy> ");
    add_history(input);
    
    /* Attempt to parse the user input */
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Lispy, &r)) {
      /* On success print and delete the AST */
      mpc_ast_t* tree = r.output;
      
      lval* x= lval_eval(e, lval_read(tree));
      lval_println(x);
      lval_del(x);
      
      mpc_ast_delete(r.output);
         
    } else {
      /* Otherwise print and delete the Error */
      mpc_err_print(r.error);
      mpc_err_delete(r.error);
    }
    
    free(input);
  }
  
  /* Undefine and delete our parsers */
  mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);
  
  return 0;
}

