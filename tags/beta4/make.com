$ WRITE SYS$OUTPUT "Commencing build of ircd-hybrid-7 for VMS..."
$ WRITE SYS$OUTPUT "Examining source directory..."
$
$ FILE_SIZE = "NOSUCHFILE"
$ FILE_SIZE = F$FILE_ATTRIBUTES("[.include]setup.h", "KNOWN")
$ IF FILE_SIZE .EQS. "NOSUCHFILE"
$  THEN 
$   WRITE SYS$OUTPUT "Could not find setup.h!"
$   WRITE SYS$OUTPUT "Please read README.VMS for information on how to install"
$   WRITE SYS$OUTPUT "ircd-hybrid-7 on VMS."
$   EXIT
$ ENDIF
$
$ ON ERROR THEN GOTO ERREXIT
$
$ WRITE SYS$OUTPUT "Building in src..."
$ SET DEF [.SRC]
$ MMK
$
$ IF $STATUS
$  THEN
$   WRITE SYS$OUTPUT "Build in src failed; check above messages"
$   SET DEF [-]
$   EXIT
$ ENDIF
$
$ WRITE SYS$OUTPUT "Leaving directory src..."
$ SET DEF [-]
$ WRITE SYS$OUTPUT "Building in modules..."
$ SET DEF [.MODULES]
$ MMK
$ WRITE SYS$OUTPUT "Leaving directory modules..."
$ SET DEF [-]
$ WRITE SYS$OUTPUT "Building in tools..."
$ SET DEF [.TOOLS]
$ MMK
$ WRITE SYS$OUTPUT "Leaving directory tools..."
$ WRITE SYS$OUTPUT "Build of ircd-hybrid-7 for VMS complete!"
$
$ ERREXIT:
$ WRITE SYS$OUTPUT "Build aborted due to error"
$ SET DEF [-]
