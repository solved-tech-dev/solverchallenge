# T06: complex FGMRES with PanguLU

The configuration selects XianzhiKrylov FP64 complex FGMRES with PanguLU LU
as the preconditioner. Build and run from the project directory using the
main README commands.

The tested downloaded XianzhiKrylov26.45/DTK26.04 package returns
`UNSUPPORTED` at `config_set` for this combination. A package with the
`fgmres_complex` + `pangulu` method is required. The program reports failure
instead of running a different method or claiming a successful result.
