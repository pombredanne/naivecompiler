# -*- coding: utf-8 -*-
import logging
import sys
import ctypes
import traceback
from Structure import Structure
#from ctypes import CFUNCTYPE, POINTER, Structure, c_int, c_double, c_uint, c_char_p, c_void_p, c_ulonglong, cast


logger = logging.getLogger(__file__)

FileMD5Size = 16

## symbol map
class S_Symbol( Structure ):
    structure =
    (
        ("id", '<I'),
        ("length", "<I=len( string )"),
        ("string", "z"),
    )

class SymbolMap( Structure ):
    structure = 
    (
	('count' , '<I=data.GetSize()'), 
	('size' , '<I=data.GetDataSize()'),
	('data' , ':' , SerializeCollection ),
    )
    
## File
  # -------
  # magic
  # entry (of segment table)
  # bodySize
  # bodyMD5
  # body ------
  #      SegmentTable -----
  #          SymbolMapEntry
  #          ASTEntry
  #          --------------
  # ---------
  # SymbolMap
  # ------
  # ASTEntry
  # -------
class FileFormat(Structure):
    structure = (
        ( "magic", "<I"),
        ( "entry", "<I=4 * 3 + 16"),
        ( "bodySize", "<I=len(body)"),
        ( "bodyMD5", "%ds" % FileMD5Size),
        ( "body", ":"),
    )

    
class S_CodeBlock(Structure):
    structure = (
        ( "type", "<I=1"),
        ( "declaration_list", ":", S_DeclarationList),
        ( "statement_list", ":", S_StatementList)
    )

class S_DeclarationList(Structure):
    structure = (
        ( "type", "<I=2"),
        ( "count", "<I"),
        ( "size", "<I=len(data)"),
        ( "data", ":"), # for declaration in DeclarationList: data+=str(declaration)
    )

class S_StatementList(Structure):
    structure = (
        ( "type", "<I=3"),
        ( "count", "<I"),
        ( "size", "<I=len(data)"),
        ( "data", ":"), # for stmt in StatementList: data+=str(stmt)
    )

class S_Declaration(Structure):
    structure = (
        ( "type", "<I=4"),
        ( "_type", "<I"), # int : 0, double : 1
        ( "id", "<I"),
    )


class S_ForStmt(Structure):
    structure = (
        ( "type", "<I=5"),
        ( "assigment_expr", ":"),
        ( "bool_expr", ":"),
        ( "assigment_expr", ":")
    )

class S_ReadStmt(Structure):
    structure = (
        ("type", "<I=6"),
        ("id", "<I"),
    )   

class S_WriteStmt(Structure):
    structure = (
        ("type", "<I=7"),
        ("id", "<I"),
    )

class S_AssignmentExpr(Structure):
    structure = (
        ("type", "<I=8"),
        ("id", "<I"),
        ("exp_size": "<I=len(body)"),
        ("exp", ":")
    )

class S_BinaryOp(Structure):
    structure = (
        ("type", "<I=9"),
        ("op", "c"),
        ("exp1_size", "<I=len(exp1)"),
        ("exp2_size": "<I=len(exp2)"),
        ("exp1", ":")
        ("exp2", ":")
    )

class S_ID(Structure):
    structure = (
        ("type", "<I=10"),
        ("_id", "<I"), # id in StringTable
    )

class S_Number(Structure):
    structure = (
        ("type", "<I=11"),
        ("_id", "<I"),
    )
