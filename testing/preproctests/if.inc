@ This file is included by several tests, each with different command line 
@ options.

*if score
*comment score
*else
*comment not score
*fi

*if part
*comment part
*else
*comment not part
*fi

*if stave 1
*comment only stave 1
*else
*comment not only stave 1
*fi

*if staves 1-2,3
*comment exactly staves 1-2,3
*else
*comment not exactly staves 1-2,3
*fi

*if myformat
*comment myformat
*else
*comment not myformat
*fi

*if format
*comment any format
*else
*comment not any format
*fi

*if part
  *if myformat
  *comment part and myformat
  *else
  *comment part and not myformat
  *fi
*fi

*if score or myformat
*comment score or myformat
*else
*comment neither score nor myformat
*fi

*if not score or myformat
*comment either not score or myformat
*else
*comment score and not myformat
*fi

*if undef x
*comment macro x is not defined
*else
*comment macro x is defined
*fi

*comment defining macro x
*define x XXX

*if undef x
*comment macro x is not defined
*else
*comment macro x is defined
*fi

@ End      
