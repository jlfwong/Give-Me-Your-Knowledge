/*
 * Spring 2010 starter code for CS241 assignments 9 and 10.
 * Last modified: 29 Nov 2009 by JCBeatty.
 *
 * Based on Scheme code by Gord Cormack. Java translation by Ondrej Lhotak.
 * C translation by Brad Lushman with further modifcations by John Beatty.
 *
 * Cautionary reminder: any C compiler regards -> and [] as operators having the same precedence that
 * associate from left to right. Both have higher precedence than the dereferencing operator *.
 * Hence an expression such as
 *
 *     (*(tokens->ptrArray))[idx]
 *
 * which treats tokens->ptrArray as a pointer to a block of memory containing an array of something
 * (and retrieves one of those somethings) can be written more simply as
 *
 *     (*tokens->ptrArray)[idx]
 *
 * but NOT as
 *
 *     *tokens->ptrArray[idx]
 *
 * because this expression is parsed as
 *
 *     *( (tokens->ptrArray)[idx] )
 *
 * which treats tokens->ptrArray as an array of pointers (and retrieves whatever one of those addresses
 * points to) -- a very different thing.
 */

// -----------------------------------------------------------------------------------------------------
//
// Headers.
//
// -----------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>

// Dmalloc (http://dmalloc.com) provides cover routines for memory management and string routines that
// catch many ptr errors, array over/underwrites, and memory leaks. For information regarding the use of
// dmalloc in CS 241 see http://www.student.cs.uwaterloo.ca/~jcbeatty/cs241/documentation/index.html.
// Quickstart instructions: (1) make dmallocFor241.h accessible to the compiler; (2) make the library
// libdmalloc.a accessible to the compiler; (3) compile with -DDMALLOC and -DDMALLOC_FUNC_CHECK.
// Note 1: this particular program will write dmalloc output to stderr if you also compile with -DIDE.
// [See the call to dmalloc_setup(...) in main(...).]
// Note 2: marmoset will NOT define these macros when it compiles a submission and will NOT load libdmalloc.a.
#ifdef DMALLOC
   #include "dmallocFor241.h"
#endif

// Print debugging output if true.
#define DEBUG false

// -----------------------------------------------------------------------------------------------------
//
// Prototypes.
//
// -----------------------------------------------------------------------------------------------------

typedef struct StringPtrArray_ * StringPtrArrayPtr;
typedef struct Tree_           * TreePtr;

void bail(                StringPtrArrayPtr rule );    // Reports a failure in pass 1 or 2 to match a rule. Should never happen.
void panicExit(           char            * rule );    // Report a more general disaster and exit the program, reporting a non-zero status code.

void printStringPtrArray( StringPtrArrayPtr spa  );    // For debugging only.
void printTreeNode(       TreePtr           tree );    // For debugging only.
void printTree(           TreePtr           tree );    // For debugging only.

int                 main(              int argc, char * argv[]                     );
TreePtr             readParse(         char * grammarSymbol                        );    // Pass one.
StringPtrArrayPtr   symbolsDeclaredIn( TreePtr tree                                );    // Pass two.
char              * generateCodeFor(   TreePtr tree, StringPtrArrayPtr symbolTable );    // Pass three.

// -----------------------------------------------------------------------------------------------------
//
// Terminal symbols.
//
// -----------------------------------------------------------------------------------------------------

/* The set of terminal symbols in the WL grammar. */
char * terminals[] = { "BOF", "BECOMES", "COMMA", "ELSE", "EOF", "EQ", "GE",
    "GT", "ID", "IF", "INT", "LBRACE", "LE", "LPAREN", "LT", "MINUS", "NE",
    "NUM", "PCT", "PLUS", "PRINTLN", "RBRACE", "RETURN", "RPAREN", "SEMI",
    "SLASH", "STAR", "WAIN", "WHILE" };

bool isTerminal( char *sym ) {
    int idx;
    for( idx=0; idx < sizeof(terminals)/sizeof(char*); idx++ )
        if( ! strcmp(terminals[idx],sym) ) return 1;
    return 0;
}

// -----------------------------------------------------------------------------------------------------
//
// Support for a simple-minded proto-symbol table.
//
// -----------------------------------------------------------------------------------------------------

// The elements in an array are (*ptrArray)[0] ... (*ptrArray)[size-1].
typedef struct StringPtrArray_ {
    int    size;           // # of pointers in the array.
    char * (*ptrArray)[];  // ptrArray is a pointer to an array of pointers to C-strings (char*'s).
} StringPtrArray;

// Appends the pointers in the second array to the first array and deallocates the second.
// Doing so requires enlarging the memory block allocated to hold the array.
void stringArrayAppend( StringPtrArray * this, StringPtrArray * other ) {

    int idx;

    // Expand the space allocated for this->ptrArray sufficiently to hold the contents of other.
    // This may, or may not, required releasing the original block and allocating a new, larger one.
    // In the latter case, realloc copies the contents of the old block to the beginning of the new block.
    this->ptrArray = realloc( this->ptrArray, (this->size + other->size) * sizeof(char*) );

    // Append the elements of other->ptrArray to the end of this->ptrArray.
    for( idx = 0; idx < other->size; idx++ ) {
        (*(this->ptrArray))[this->size + idx] = (*other->ptrArray)[idx];
    }

    this->size += other->size;

    free( other->ptrArray );
    free( other           );      // [JCB - memory leak fixed.]
}

void freeStringPtrArray( StringPtrArray *  strs ) {
    int idx;
    for( idx = 0; idx < strs->size; idx++ ) {
        free( (*strs->ptrArray)[idx] );
    }
    free( strs->ptrArray );
    free( strs           );
}

// -----------------------------------------------------------------------------------------------------
//
// Support for representing and working with a parse tree.
//
// -----------------------------------------------------------------------------------------------------

// Data structure for representing the parse tree. Abstractly, leaves are terminals & internal nodes are
// non-terminals. However, in the data structure used here epsilon rules break this abstraction.
// In particular, the node for the lhs of an epsilon rule is, of course, a non-terminal but the node
// has no children (ie .nChildren == 0 and .children == NULL). In other words, we do not explicitly
// represent epsilon by a node in the tree.
//
// Details: (1) leaves of the tree have nChildren == 0, but rule has TWO elements, the second of which is the
// string scanned that was recognized as an instance of this terminal symbol.
//
// (2) A Tree struct has a fixed size (for most compilers, 12 bytes). In particular, the .children field
// is a pointer. For leaves it will be NULL; for internal nodes it will point to a block of heap memory
// containing an array of pointers to other tree nodes.
//
// (3) A leaf corresponds EITHER to a terminal node OR to a non-terminal which is the LHS of an epsilon rule
// (which has no children).
//
// (4) One weirdness: for terminals the rule field consists not just of the terminal symbol, but has the form
// { <terminal>, <lexemeForTerminal> } - eg { INT, int } or { ID, foo } or { NUM, 42 } or { WAIN, wain } -
// because for some terminals (ie INT and ID) you must retain the originating lexical string somewhere,
// and this is as good a place as any.
//
// (5) The primary use for .rule is to compare it against the literal representation of a rule as a string
// -- eg "S BOF procedure EOF" -- in passTwo(...) or passThree(...). At first blush it therefore seems perverse
// and inefficient to represent rule as a list of token strings -- eg { "S", "BOF", "procedure", "EOF" } --
// as we then have to repeatedly tokenize the rule literals to compare them with values of .rule. Arguably,
// however, doing so makes the compiler a bit more robust in that it doesn't care how much whitespace exists
// between the tokens in such literal rule strings. And the programs we compile are generally small enough
// that the additional overhead can be forgiven.

typedef struct Tree_ {
    StringPtrArray * rule;            // Entry [0] is the LHS; entries [1] ... [nChildren] are the RHS.
    int              nChildren;
    struct Tree_   * (*children)[];   // children is a pointer to an ARRAY of pointers to (other) Tree structs.
} Tree;

void freeTree( TreePtr tree ) {
    int idx;
    freeStringPtrArray( tree->rule );
    if( tree->nChildren > 0 ) {
        for( idx=0; idx < tree->nChildren; idx++ ) {
            freeTree( (*tree->children)[idx] );
        }
        free( tree->children );
    }
    free( tree );
}

// Divide a string into a sequence of tokens.
// Warning: strtok(...) modifies the string it's scanning; see man 3 strtok for details.
// Applied to lines read from the *.wli filee supplied as input to the compiler.
StringPtrArray * tokenize( char * line ) {
    char           * nextToken;
    StringPtrArray * tokens = malloc( sizeof(StringPtrArray) );
    tokens->size            = 0;
    tokens->ptrArray        = NULL;
    tokens->ptrArray        = malloc( strlen(line) * sizeof(char*) );                  // # of tokens in line <= strlen(line)
    nextToken = strtok( line, " \n" );
    do {
        (*tokens->ptrArray)[tokens->size] = (char*) malloc( strlen(nextToken) + 1 );   // +1 for the terminating '\0'
        strcpy( (*tokens->ptrArray)[tokens->size], nextToken );
        tokens->size++;
    } while( (nextToken = strtok(NULL," \n")) );
    return tokens;
}

/* Does this node's rule match otherRule? */
bool doesTreeNodeMatchRule( TreePtr tree, char * otherRule ) {

    int    result     = true;
    char * copyOfRule = (char*) malloc( strlen(otherRule) + 1  );

    // Tokenize a COPY of otherRule because strtok(...) is destructive. If we did not make a copy first,
    // we would be changing the actual value of a literal string in symbolsDeclaredIn(...) or genCode(...)!
    StringPtrArray * tokens;
    strcpy( copyOfRule, otherRule );
    tokens = tokenize( copyOfRule );

    if( tree->rule->size != tokens->size ) { 
        result = false;
    } else {
        int idx;
        for( idx=0; idx < tree->rule->size; idx++ ) {
            if( strcmp( (*tree->rule->ptrArray)[idx], (*tokens->ptrArray)[idx]) ) {
                result = false;
                break;
            }
        }
    }   

    freeStringPtrArray( tokens );
    free( copyOfRule );
    return result;
}

// -----------------------------------------------------------------------------------------------------
//
// Main.
//
// -----------------------------------------------------------------------------------------------------

int main( int argc, char * argv[] ) {

    Tree           * parseTree;    // Reconstructed from a *.wli file.
    StringPtrArray * symbols;      // The symbol table.
    char           * program;      // An assembly language equivalent to the WL program being compiled.

    #if defined(IDE) && defined(DMALLOC)
        // 1st param - one of: DMALLOC_runtimeFor241, DMALLOC_lowFor241, DMALLOC_mediumFor241, DMALLOC_highFor241.
        // 2nd param - most useful possibilities are: print-messages, log=logfile, inter=100, and log-trans
        //             (logs all calls to dmalloc cover routines). Separate these tokens by commas when you
        //             want to supply more than one - eg "print-messages,inter=10".
        dmalloc_setup( DMALLOC_highFor241, "print-messages" );
    #endif

    // argv[0] is always the name of the program being run.
    // argv[1], when present, is assumed to be a path to an input wli file.
    if( argc == 2 ) {
        if( DEBUG ) fprintf( stderr, "argv[0] = %s\nargv[1] = %s\n", argv[0], argv[1] );
        // Unfortunately we can't just assign a newly opened file pointer to stdin on Solaris, so we do it this way...
        FILE * fp = fopen( argv[1], "r" );
        if( fp == NULL ) panicExit( "can't open the specified input file" );
        int fd = fileno(fp);
        if( dup2(fd,STDIN_FILENO) ) panicExit( "failed to replace stdin by the specified input file" );
        close(fd);
    };

    parseTree = readParse( "S" );                         // Read a *.wli input file, (re)building the program's parse tree.
    if( DEBUG )  {
        fputc( '\n', stderr );
        printTree( parseTree );
        fputc( '\n', stderr );
    }
    symbols   = symbolsDeclaredIn( parseTree );           // Walk the tree, building a list of the variables declared in it.
    program   = generateCodeFor( parseTree, symbols );    // Walk the parse tree, generating code. [JCB - memory leak fixed.]
    printf( program );

    free( program );
    freeTree( parseTree );
    freeStringPtrArray( symbols );

    #if defined(DMALLOC) && false
        // Actually dmalloc_shutdown(...) is called automatically at exit. But sometimes when debugging it's
        // useful to dump the list of unfreed memory *before* exiting so we can poke around with a debugger.
        dmalloc_shutdown();
    #endif

    fclose( stdin );
    return 0;
}

// -----------------------------------------------------------------------------------------------------
//
// Pass one - building the parse tree.
//
// -----------------------------------------------------------------------------------------------------

// Read a *.wli file, reconstructing and returning the program's parse tree.
TreePtr readParse( char * grammarSymbol ) {

    int              idx;
    char             line[256];
    StringPtrArray * tokens;
    TreePtr          tree;

    fgets( line, 256, stdin );
    tokens = tokenize( line );

    tree = malloc( sizeof(Tree) );
    tree->rule      = tokens;
    tree->nChildren = 0;
    tree->children  = NULL;

    // tokens.size is always > 0.
    // If tokens.size == 1 we have an eps-rule (==> token 1 is a variable and there are no children)
    // If tokens.size == 2 then either we have a terminal (==> no children) or we have a chain rule A --> B (==> one child).
    // For all other cases there are children.
    if( tokens->size > 1 && (! isTerminal(grammarSymbol)) ) {
        tree->children  = malloc( (tokens->size - 1) * sizeof(Tree*) );  // -1 because there's no entry in children for the LHS, which is (*tokens->ptrArray)[0].
        for( idx = 1; idx < tokens->size; idx++ ) {
            char * next = (*(tokens->ptrArray))[idx];
            (*tree->children)[tree->nChildren] = readParse( next );
            tree->nChildren++;
        }
    }
    return tree;
}

// -----------------------------------------------------------------------------------------------------
//
// Pass two - building a symbol table.
//
// -----------------------------------------------------------------------------------------------------

/* Build a list (actually an array) of symbols defined in tree by walking it. */
StringPtrArray * symbolsDeclaredIn( TreePtr tree ) {

    if( doesTreeNodeMatchRule(tree,"S BOF procedure EOF") ) {
        /* Recurse on procedure */
        return symbolsDeclaredIn( (*tree->children)[1] );
    }
    else if( doesTreeNodeMatchRule(tree,"procedure INT WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") ) {
        StringPtrArray * ret = (StringPtrArray*) malloc( sizeof(StringPtrArray) );
        ret->ptrArray        = NULL;
        ret->size            = 0;

        /* Recurse on dcl and dcl */
        stringArrayAppend( ret, symbolsDeclaredIn( (*tree->children)[3] ) );
        stringArrayAppend( ret, symbolsDeclaredIn( (*tree->children)[5] ) );
        return ret;
    }
    else if( doesTreeNodeMatchRule(tree,"dcl INT ID") ) {
        /* Recurse on ID */
        return symbolsDeclaredIn( (*tree->children)[1] );
    }
    else if( ! strcmp( (*tree->rule->ptrArray)[0], "ID") ) {
        StringPtrArray * ret = (StringPtrArray*) malloc( sizeof(StringPtrArray) );
        ret->size            = 1;
        ret->ptrArray        = malloc( sizeof(char*)              );
        (*ret->ptrArray)[0]  = malloc( strlen((*tree->rule->ptrArray)[1]) + 1 );
        strcpy( (*ret->ptrArray)[0], (*tree->rule->ptrArray)[1] );
        return ret;
    }
    else bail( tree->rule );
    return NULL;                // Should never get here.
}

// -----------------------------------------------------------------------------------------------------
//
// Pass three - code generation.
//
// -----------------------------------------------------------------------------------------------------

/* Generate the code for the parse tree. */
char * generateCodeFor( TreePtr tree, StringPtrArray * symbolTable  ) {

    if( doesTreeNodeMatchRule(tree,"S BOF procedure EOF") ) {
        char * rec = generateCodeFor( (*tree->children)[1], symbolTable );
        char * ret = malloc( strlen(rec) + 8 );
        strcpy( ret, rec        );
        strcat( ret, "jr $31\n" );
        free( rec );
        return ret;
    }
    else if( doesTreeNodeMatchRule(tree,"procedure INT WAIN LPAREN dcl COMMA dcl RPAREN LBRACE dcls statements RETURN expr SEMI RBRACE") ) {
        return generateCodeFor( (*tree->children)[11], symbolTable );
    }
    else if( doesTreeNodeMatchRule(tree,"expr term") ) {
        return generateCodeFor( (*tree->children)[0], symbolTable );
    }
    else if( doesTreeNodeMatchRule(tree,"term factor") ) {
        return generateCodeFor( (*tree->children)[0], symbolTable );
    }
    else if( doesTreeNodeMatchRule(tree,"factor ID") ) {
        return generateCodeFor( (*tree->children)[0], symbolTable );
    }
    else if( ! strcmp((*tree->rule->ptrArray)[0], "ID") ) {
        char *name = (*tree->rule->ptrArray)[1];
        char *ret  = (char*) malloc(14);
        if( ! strcmp( name, (*symbolTable->ptrArray)[0]) ) strcpy( ret, "add $3,$0,$1\n" );
        if( ! strcmp( name, (*symbolTable->ptrArray)[1]) ) strcpy( ret, "add $3,$0,$2\n" );
        return ret;
    }
    else bail( tree->rule );
    return 0;
}

// -----------------------------------------------------------------------------------------------------
//
// Misc helpers.
//
// -----------------------------------------------------------------------------------------------------

// Print an error message regarding an un-recogized rule and exit the program, reporting a non-zero status code.
void bail( StringPtrArray * rule ) {
    int i;
    fprintf(stderr, "ERROR: unrecognized rule");
//    for (i=0; i < msg.size; i++) fprintf(stderr, " %s", msg.ptrArray[i]);
    for( i = 0; i < rule->size; i++ ) {
        fprintf( stderr, " %s", (*rule->ptrArray)[i]);
    }
    fprintf(stderr, "\n");
    exit(1);
}

// Report a more general disaster and exit the program, reporting a non-zero status code.
void panicExit( char * rule ) {
    fprintf( stderr, "ERROR: %s\n.", rule );
    exit(1);
}

// -----------------------------------------------------------------------------------------------------
//
// Useful when debugging.
//
// -----------------------------------------------------------------------------------------------------

void printStringPtrArray( StringPtrArray * spa ) {
    if( spa == NULL ) {
        fprintf( stderr, "ptr is <null>\n" );
    } else {
        fprintf( stderr, "%d: ", spa->size );
        int idx;
        for( idx = 0; idx < spa->size; idx ++ ) {
            fprintf( stderr, "%s ", (*spa->ptrArray)[idx] );
        }
        fprintf( stderr, "\n" );
    }
}

void printTreeNode( TreePtr tree ) {
    fprintf( stderr, "\n" );
    if( tree == NULL ) {
        fprintf( stderr, "ptr is <null>\n" );
    } else {
        fprintf( stderr, ".rule      = " );
        printStringPtrArray( tree->rule );
        fprintf( stderr, ".nChildren = %d\n", tree->nChildren );
        int idx;
        for( idx = 0; idx < tree->nChildren; idx++ ) {
            fprintf( stderr, "    %2d: ", idx );
            TreePtr child = (*(tree->children))[idx];
            printStringPtrArray( child->rule );
        }
    }
}

void printTree( TreePtr tree ) {
    if( tree == NULL ) {
        return;
    } else {
        printStringPtrArray( tree->rule );
        int idx;
        for( idx = 0; idx < tree->nChildren; idx++ ) {
            printTree( (*(tree->children))[idx] );
        }
    }
}

// Will be compiled only if DMALLOC is defined (eg "gcc -DDMALLOC...")
#ifdef DMALLOC
int bytesForHeapPtr( void * heapPtr ) {
    DMALLOC_SIZE    userSize;        // Bytes allocated to hold data.
    DMALLOC_SIZE    totalSize;       // Total bytes allocated, including administrative overhead.
    // The other parameters, if not null, return other info we're not interested in.
    if( dmalloc_examine( heapPtr, &userSize, &totalSize, NULL, NULL, NULL, NULL, NULL ) == DMALLOC_ERROR ) {
        fprintf( stderr, "dmalloc_examine error." );
        return -1;
    } else {
        return userSize;
    }
}
#endif
