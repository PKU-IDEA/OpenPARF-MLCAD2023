/* $Id: parser.yy 48 2009-09-05 08:07:10Z tb $ -*- mode: c++ -*- */
/** \file parser.yy Contains the example Bison parser source */

%{ /*** C/C++ Declarations ***/

#include <stdio.h>
#include <string>
#include <vector>
#include <array>

%}

/*** yacc/bison Declarations ***/

/* Require bison 2.3 or later */
%require "2.3"

/* add debug output code to generated parser. disable this for release
 * versions. */
%debug

/* start symbol is named "start" */
/*%start start*/

/* write out a header file containing the token defines */
%defines

/* use newer C++ skeleton file */
%skeleton "lalr1.cc"

/* namespace to enclose parser in */
%define api.namespace {ehbookshelfparser}

/* set the parser's class identifier */
%define api.parser.class {Parser}

/* keep track of the current position within the input */
%locations
%initial-action
{
    // initialize the initial location object
    @$.begin.filename = @$.end.filename = &driver.streamname;
};

/* The driver is passed by reference to the parser and to the scanner. This
 * provides a simple but effective pure interface, not relying on global
 * variables. */
%parse-param { class Driver& driver }

/* verbose error messages */
%define parse.error verbose

 /*** BEGIN EXAMPLE - Change the example grammar's tokens below ***/

%union {
    unsigned      intVal;
    double        dblVal;
    struct {
        long value;
        long bits;
    } mask;
    std::string*  strVal;
}

%token                ENDF       0  "end of file"
%token                ENDL
%token                BIT_MASK;
%token                OCT_MASK;
%token                DEC_MASK;
%token                HEX_MASK;
%token    <intVal>    INT
%token    <dblVal>    DOUBLE
%token    <strVal>    STRING
%token                KWD_BEGIN
%token                KWD_END
%token                KWD_SITEMAP
%token                KWD_SLICEL
%token                KWD_SLICEM
%token                KWD_SLICE
%token                KWD_DSP
%token                KWD_BRAM
%token                KWD_IO
%token                KWD_SITE
%token                KWD_RESOURCES
%token                KWD_FIXED
%token                KWD_CELL
%token                KWD_PAR
%token                KWD_PIN
%token                KWD_INPUT
%token                KWD_OUTPUT
%token                KWD_CLOCK
%token                KWD_CTRL
%token                KWD_CTRL_SR
%token                KWD_CTRL_CE
%token                KWD_INPUT_CLOCK
%token                KWD_INPUT_CTRL
%token                KWD_CAS
%token                KWD_NET
%token                KWD_ENDNET
%token                KWD_CLOCKREGION
%token                KWD_CLOCKREGIONS
%token                KWD_SHAPE
%token                KWD_TYPE
%token                KWD_REGION_CONSTRAINT
%token                KWD_REGION_MAPPING

%token    <strVal>    LIB_FILE
%token    <strVal>    SCL_FILE
%token    <strVal>    NODE_FILE
%token    <strVal>    NET_FILE
%token    <strVal>    PL_FILE
%token    <strVal>    WT_FILE
%token    <strVal>    SHAPE_FILE

%type<mask> BIT_MASK OCT_MASK DEC_MASK HEX_MASK

%destructor { delete $$; } STRING

 /*** END EXAMPLE - Change the example grammar's tokens above ***/

%{

#include "ehbookshelf_driver.h"
#include "ehbookshelf_scanner.h"

/* this "connects" the bison parser in the driver to the flex scanner class
 * object. it defines the yylex() function call to pull the next token from the
 * current lexer object of the driver context. */
#undef yylex
#define yylex driver.scanner->lex

%}

%% /*** Grammar Rules ***/

/***** top patterns *****/
top : ENDL_STAR sub_top
    ;

sub_top :
        | lib_top
        | scl_top
        | node_top
        | pl_top
        | net_top
        | cascade_shape_top
        | cascade_shape_instance_top
        | region_top
        | macros_top
        ;

/***** lib file *****/
lib_top : cell_blocks
        ;

cell_blocks : cell_blocks cell_block
            | cell_block
            ;

cell_block : cell_block_header
             cell_block_lines
             cell_block_footer
           ;

cell_block_header : KWD_CELL STRING ENDL { driver.addCellCbk(*$2); delete $2; }
                  ;

cell_block_footer : KWD_END KWD_CELL ENDL_STAR
                  ;

cell_block_lines  : cell_block_lines cell_block_line
                  | cell_block_line
                  ;

cell_block_line : KWD_PIN STRING KWD_INPUT ENDL             { driver.addCellInputPinCbk(*$2);    delete $2; }
                | KWD_PIN STRING KWD_OUTPUT ENDL            { driver.addCellOutputPinCbk(*$2);   delete $2; }
                | KWD_PIN STRING KWD_INPUT KWD_CLOCK ENDL   { driver.addCellClockPinCbk(*$2);    delete $2; }
                | KWD_PIN STRING KWD_INPUT_CLOCK ENDL       { driver.addCellClockPinCbk(*$2);    delete $2; }
                | KWD_PIN STRING KWD_INPUT KWD_CTRL ENDL    { driver.addCellCtrlPinCbk(*$2);     delete $2; }
                | KWD_PIN STRING KWD_INPUT_CTRL ENDL        { driver.addCellCtrlPinCbk(*$2);     delete $2; }
                | KWD_PIN STRING KWD_INPUT KWD_CTRL_SR ENDL { driver.addCellCtrlSRPinCbk(*$2);   delete $2; }
                | KWD_PIN STRING KWD_INPUT KWD_CTRL_CE ENDL { driver.addCellCtrlCEPinCbk(*$2);   delete $2; }
                | KWD_PIN STRING KWD_INPUT KWD_CAS ENDL     { driver.addCellInputCasPinCbk(*$2); delete $2; }
                | KWD_PIN STRING KWD_OUTPUT KWD_CAS ENDL    { driver.addCellInputCasPinCbk(*$2); delete $2; }
                | KWD_PAR STRING ENDL                       { driver.addCellParameterCbk(*$2);   delete $2; }
                ;


/*! scl file
 *  Clock region block is optional, since ISPD16 benchmark doesn't have clock regions
 */
scl_top : site_blocks rsrc_block sitemap_block
        | site_blocks rsrc_block sitemap_block clock_region_block
        ;

/* site blocks */
site_blocks : site_blocks site_block
            | site_block
            ;

site_block : site_block_header
             site_block_lines
             site_block_footer
             | site_block_header
             site_block_footer
           ;

site_block_header : KWD_SITE site_type_name ENDL
                  ;

site_type_name : KWD_SLICEL { driver.setSiteTypeCbk("SLICEL"); }
               | KWD_SLICEM { driver.setSiteTypeCbk("SLICEM"); }
               | KWD_SLICE { driver.setSiteTypeCbk("SLICE"); }
               | KWD_DSP { driver.setSiteTypeCbk("DSP"); }
               | KWD_BRAM { driver.setSiteTypeCbk("BRAM"); }
               | KWD_IO { driver.setSiteTypeCbk("IO"); }
               | STRING { driver.setSiteTypeCbk(*$1); delete $1; }
               ;

site_block_footer : KWD_END KWD_SITE ENDL_STAR { driver.endSiteBlockCbk(); }
                  ;

site_block_lines : site_block_lines site_block_line
                 | site_block_line
                 ;

site_block_line : rsrc_type_name INT ENDL { driver.setSiteNumResourcesCbk($2); }
                ;

rsrc_type_name : STRING { driver.setResourceTypeCbk(*$1); delete $1; }
               | KWD_IO { driver.setResourceTypeCbk("IO"); }
               | KWD_BRAM { driver.setResourceTypeCbk("BRAM"); }
               ;

/* resources block */
rsrc_block : rsrc_block_header
             rsrc_block_lines
             rsrc_block_footer
           ;

rsrc_block_header : KWD_RESOURCES ENDL
                  ;

rsrc_block_footer : KWD_END KWD_RESOURCES ENDL_STAR { driver.endResourceTypeBlockCbk(); }
                  ;

rsrc_block_lines : rsrc_block_lines rsrc_block_line
                 | rsrc_block_line
                 ;

rsrc_block_line : rsrc_type_name cell_name_list ENDL {
                driver.addResourceTypeCbk();
                }
                ;

cell_name_list : cell_name_list STRING { driver.addToCellNameListCbk(*$2); delete $2; }
               | STRING { driver.addToCellNameListCbk(*$1); delete $1; }
               ;

/* sitemap block */
sitemap_block : sitemap_block_header
                sitemap_block_lines
                sitemap_block_footer
              ;

sitemap_block_header : KWD_SITEMAP INT INT ENDL { driver.initSiteMapCbk($2, $3); };

sitemap_block_footer : KWD_END KWD_SITEMAP ENDL_STAR { driver.endSiteMapCbk(); }
                     ;

sitemap_block_lines : sitemap_block_lines sitemap_block_line
                    | sitemap_block_line
                    ;

sitemap_block_line : INT INT KWD_SLICEL ENDL { driver.setSiteMapEntryCbk($1, $2, "SLICEL"); }
                   | INT INT KWD_SLICEM ENDL { driver.setSiteMapEntryCbk($1, $2, "SLICEM"); }
                   | INT INT KWD_SLICE ENDL { driver.setSiteMapEntryCbk($1, $2, "SLICE"); }
                   | INT INT KWD_DSP ENDL    { driver.setSiteMapEntryCbk($1, $2, "DSP"); }
                   | INT INT KWD_BRAM ENDL   { driver.setSiteMapEntryCbk($1, $2, "BRAM"); }
                   | INT INT KWD_IO ENDL     { driver.setSiteMapEntryCbk($1, $2, "IO"); }
                   | INT INT STRING ENDL     { driver.setSiteMapEntryCbk($1, $2, *$3); delete $3; }
                   ;

/* clock region block */
clock_region_block : clock_region_block_header
                     clock_region_block_lines
                     clock_region_block_footer
                   ;

clock_region_block_header : KWD_CLOCKREGIONS INT INT ENDL { driver.initClockRegionsCbk($2, $3); }
                          ;

clock_region_block_footer : KWD_END KWD_CLOCKREGIONS ENDL_STAR
                          ;

clock_region_block_lines : clock_region_block_lines clock_region_block_line
                         | clock_region_block_line
                         ;

clock_region_block_line : KWD_CLOCKREGION STRING ':' INT INT INT INT INT INT ENDL { driver.addClockRegionCbk(*$2, $4, $5, $6, $7, $8, $9); delete $2; }
                        ;


/***** node file *****/
node_top : node_lines
         ;

node_lines : node_lines node_line
           | node_line
           ;

node_entry : STRING STRING { driver.addNodeCbk(*$1, *$2); delete $1; delete $2; }
           | node_entry BIT_MASK
           | node_entry OCT_MASK
           | node_entry DEC_MASK
           | node_entry HEX_MASK
          ;

node_line : node_entry ENDL_STAR
          ;

/***** pl file *****/
pl_top : pl_lines
          ;

pl_lines : pl_lines pl_line
         | pl_line
         ;

pl_line : STRING INT INT INT KWD_FIXED ENDL_STAR { driver.setFixedNodeCbk(*$1, $2, $3, $4); delete $1; }
        ;

/***** net file *****/
net_top : net_blocks
        ;

net_blocks : net_blocks net_block
           | net_block
           ;

net_block : net_block_header
            net_block_lines
            net_block_footer
          ;

net_block_header : KWD_NET STRING INT ENDL { driver.addNetCbk(*$2, $3); delete $2; }
                 ;

net_block_footer : KWD_ENDNET ENDL_STAR
                 ;

net_block_lines : net_block_lines net_block_line
                | net_block_line
                ;

net_block_line : STRING STRING ENDL { driver.addPinCbk(*$1, *$2); delete $1; delete $2; }
               ;

/***** cascade shape file *****/
cascade_shape_top : cascade_shape_blocks
                  ;

cascade_shape_blocks : cascade_shape_blocks cascade_shape_block
                     | cascade_shape_block
                     ;

cascade_shape_block : cascade_shape_block_header
                      cascade_shape_block_begin
                       cascade_shape_block_lines
                       cascade_shape_block_end
                     ;

cascade_shape_block_header : KWD_SHAPE STRING INT INT ENDL{ driver.beginCascadeShapeCbk(*$2, $3, $4); delete $2; }
                           ;

cascade_shape_block_begin : KWD_BEGIN ENDL
                          ;

cascade_shape_block_lines : cascade_shape_block_lines cascade_shape_block_line
                          | cascade_shape_block_line
                          ;

cascade_shape_block_line :  cascade_shape_block_line_begin
                            cascade_shape_block_line_items
                            cascade_shape_block_line_end
                          ;

cascade_shape_block_line_begin : { driver.beginCascadeShapeLineCbk(); }

cascade_shape_block_line_items : cascade_shape_block_line_items cascade_shape_block_line_item
                               | cascade_shape_block_line_item
                               ;

cascade_shape_block_line_item : STRING { driver.addCascadeShapeLineEntryCbk(*$1); delete $1; }

cascade_shape_block_line_end : ENDL { driver.endCascadeShapeLineCbk(); }

cascade_shape_block_end : KWD_END ENDL_STAR { driver.endCascadeShapeCbk(); }
                        ;

/***** cascade shape instance file *****/

cascade_shape_instance_top : cascade_shape_instance_blocks
                           ;

cascade_shape_instance_blocks : cascade_shape_instance_blocks cascade_shape_instance_block
                              | cascade_shape_instance_block
                              ;

cascade_shape_instance_block : cascade_shape_instance_block_header
                               cascade_shape_instance_block_begin
                                cascade_shape_instance_block_lines
                                cascade_shape_instance_block_end
                              ;

cascade_shape_instance_block_header : STRING INT INT STRING ENDL { driver.beginCascadeShapeInstanceCbk(*$1, $2, $3, *$4); delete $1; delete $4; }

cascade_shape_instance_block_begin : KWD_BEGIN ENDL
                                   ;

cascade_shape_instance_block_lines : cascade_shape_instance_block_lines cascade_shape_instance_block_line
                                    | cascade_shape_instance_block_line
                                    ;

cascade_shape_instance_block_line : STRING ENDL { driver.addCascadeShapeInstanceEntryCbk(*$1); delete $1; }
                                  ;

cascade_shape_instance_block_end : KWD_END ENDL_STAR { driver.endCascadeShapeInstanceCbk(); }
                                 ;

/***** region file *****/

region_top : region_constraints_blocks region_mapping_blocks
           ;

region_constraints_blocks : region_constraints_blocks region_constraints_block
                          | region_constraints_block
                          ;

region_constraints_block : region_constraints_block_header
                            region_constraints_block_lines
                            region_constraints_block_footer

region_constraints_block_header : KWD_REGION_CONSTRAINT KWD_BEGIN INT INT ENDL { driver.beginRegionConstraintCbk($3, $4);}


region_constraints_block_lines: region_constraints_block_lines region_constraints_block_line
                              | region_constraints_block_line
                              ;

region_constraints_block_line : STRING INT INT INT INT ENDL { driver.addRegionConstraintEntryCbk(*$1, $2, $3, $4, $5); delete $1; }

region_constraints_block_footer : KWD_REGION_CONSTRAINT KWD_END ENDL_STAR { driver.endRegionConstraintCbk(); }

region_mapping_blocks : region_mapping_blocks region_mapping_block
                      | region_mapping_block
                      ;

region_mapping_block: region_mapping_block_header
                        region_mapping_block_lines
                        region_mapping_block_footer
                      ;

region_mapping_block_header: KWD_REGION_MAPPING KWD_BEGIN ENDL { driver.beginRegionMappingCbk();}

region_mapping_block_lines: region_mapping_block_lines region_mapping_block_line
                          | region_mapping_block_line
                          ;

region_mapping_block_line: STRING INT ENDL { driver.addRegionMappingEntryCbk(*$1, $2); delete $1; }

region_mapping_block_footer: KWD_REGION_MAPPING KWD_END ENDL_STAR { driver.endRegionMappingCbk(); }

/* macros file */

macros_top: macro_lines

macro_lines: macro_lines macro_line
           | macro_line
           ;

macro_line : STRING ENDL {driver.addMacroInstCbk(*$1); delete $1; }

/* swallow ENDL by recursion */
ENDLS : ENDLS ENDL
      | ENDL
      ;

ENDL_STAR : ENDLS
          | /* empty */
          ;

/*** END EXAMPLE - Change the example grammar rules above ***/

%% /*** Additional Code ***/

void ehbookshelfparser::Parser::error(const Parser::location_type& l, const std::string& m)
{
    driver.error(l, m);
    exit(1);
}
