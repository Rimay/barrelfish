%include polycode.fmt

%if false
  Flounder2: an even more simpler IDL for Barrelfish

  Copyright (c) 2009 ETH Zurich.
  All rights reserved.

  This file is distributed under the terms in the attached LICENSE file.
  If you do not find this file, copies can be found by writing to:
  ETH Zurich D-INFK, Universitaetstrasse 6, CH-8092 Zurich. Attn: Systems Group.
%endif


\section{General Functions for Back-ends}


In this module, we define some general functions. These functions are
used in both Code and Header back-ends. They are quite simple, so we
can afford to present them out of their context.


> module Backend where

%if false

> import Data.List

> import Syntax

%endif


\subsection{The Preamble}

In both cases, we will have to emit a long, tedious copyright
notice. So, here it is done once and for all.

> preamble :: Interface -> String -> String
> preamble interface filename =
>     let name =
>             case interface of
>                    Interface _ (Just desc) _ -> desc
>                    _ -> ""
>     in
>     {-" \mbox{and so on\ldots} "-}

%if false

>     "/*\n * Interface Definition: " ++ name ++ "\n\
>     \ * Generated from: " ++ filename ++ "\n\
>     \ * \n\
>     \ * Copyright (c) 2009, ETH Zurich.\n\
>     \ * All rights reserved.\n\
>     \ * \n\
>     \ * This file is distributed under the terms in the attached LICENSE\n\
>     \ * file. If you do not find this file, copies can be found by\n\
>     \ * writing to:\n\
>     \ * ETH Zurich D-INFK, Universitaetstrasse 6, CH-8092 Zurich.\n\
>     \ *  Attn: Systems Group.\n\
>     \ * \n\
>     \ * THIS FILE IS AUTOMATICALLY GENERATED: DO NOT EDIT!\n\
>     \ */\n\n"

%endif


\subsection{Dealing with Namespace Issues}


In order to avoid name clashes, we qualify type names with the
interface name, excepted when it is a built-in type.

> qualifyName :: String -> TypeRef -> String
> qualifyName interfaceName (Builtin t) = show t
> qualifyName interfaceName (TypeVar t) = interfaceName ++ "_" ++ t
> qualifyName interfaceName (TypeAlias t _) = interfaceName ++ "_" ++ t

When we are declarating real C types, we have to add a @_t@ at the
end.

> qualifyType :: String -> TypeRef -> String
> qualifyType qualifier (Builtin String) = "char *"
> qualifyType qualifier (TypeAlias name _) = name ++ "_t"
> qualifyType qualifier typeDef =
>     qualifyName qualifier typeDef ++ "_t"

When we are generating stubs, we will need to qualify exported procedures:
@qualifyProcName@ corresponds to the interface namespace, along with a
@_fn@ specifier.

> qualifyProcName :: String -> TypeRef -> String
> qualifyProcName interfaceName typeDef =
>     qualifyName interfaceName typeDef ++ "_fn"


\subsection{Dealing with Declarations}


Often (always), we treat types and messages separately. Hence,
@partitionTypesMessages@ takes a list of declarations and partitions
it into two lists: one containing the type declarations, the other
containing the message declarations.

> partitionTypesMessages :: [Declaration] -> ([TypeDef], [MessageDef])
> partitionTypesMessages declarations =
>     let (types, messages) = foldl' typeFilter ([],[]) declarations in
>         (types, reverse messages)
>         where typeFilter (types, messages) (Messagedef m) = (types, m : messages)
>               typeFilter (types, messages) (Typedef t) = (t : types, messages)


\subsection{Dealing with Messages}


Messages can either go Forward or Backward:

> data MessageClass = Forward
>                   | Backward

This defines the following relation:

> isForward, isBackward :: MessageDef -> Bool
> isForward (Message MResponse _ _ _) = False
> isForward _ = True
> isBackward (Message MCall _ _ _) = False
> isBackward _ = True

The distinction between \emph{service} and \emph{client} handler
stands in the type of the closure response: one is typed for the
\emph{service} closure, the other for the \emph{client} closure. This
difference is materialized by the following data-type:

> data Side = ServerSide
>           | ClientSide

> instance Show Side where
>     show ServerSide = "service"
>     show ClientSide = "client"


To compile the list of arguments of messages, we use:

> compileCommonDefinitionArgs :: String -> Side -> MessageDef -> [(String,String)]
> compileCommonDefinitionArgs interfaceName side message@(Message _ _ messageArgs _) =
>     [("struct " ++ interfaceName ++ "_" ++ show side ++ "_response *", "st")]
>  ++ [(constType typeArg ++ " " ++ qualifyType interfaceName typeArg, nameOf arg)
>      | Arg typeArg arg <- messageArgs ]

> compileRPCDefinitionArgs :: String -> [RPCArgument] -> [(String,String)]
> compileRPCDefinitionArgs interfaceName rpcArgs =
>     ("struct " ++ interfaceName ++ "_client_response *", "st" ) :
>     [ case messageArg of
>       RPCArgIn typeArg arg ->
>           (constType typeArg ++ " "
>            ++ qualifyType interfaceName typeArg,
>            nameOf arg)
>       RPCArgOut typeArg arg ->
>           (qualifyType interfaceName typeArg ++ "*",
>            nameOf arg)
>       | messageArg <- rpcArgs ]

Where \verb!const! adds a const type modifier for pointers that should
not be modified.

> constType :: TypeRef -> String
> constType (Builtin String) = "const"
> constType _ = ""



\subsection{Dealing with Dynamic Arrays}


When we manipulate dynamic arrays, we might just need the name of the
array, without its associated length bound.

> nameOf :: Variable -> String
> nameOf (Name s) = s
> nameOf (DynamicArray s _ _) = s


Conversely, when marshaling or unmarshaling dynamic arrays, we need to
pass the @length@ parameter.

> listOfArgs :: String -> Variable -> String
> listOfArgs dereference (Name s) = dereference ++ s
> listOfArgs dereference (DynamicArray name length _) = dereference ++ name ++ ", " ++ length


> callNameOf :: MessageDef -> String
> callNameOf (Message _ messageName _ _) = messageName
> callNameOf (RPC name _ _) = name
