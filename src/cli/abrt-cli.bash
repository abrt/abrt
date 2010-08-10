# bash-completion add-on for abrt-cli(1)
# http://bash-completion.alioth.debian.org/

# $1 = additional options for abrt-cli
_abrt_list()
{
    echo $(abrt-cli --list $1 | grep UUID | awk '{print $3}')
    return 0
}

_abrt_cli()
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    opts="--help --version --list --report --delete"

    #
    #  Complete the arguments to some of the basic commands.
    #
    case "${prev}" in
        --list)
            opts="--full"
            ;;
        --report)
            # Include only not-yet-reported crashes.
            opts="--always $(_abrt_list)"
            ;;
        --always) # This is for --report --always
            # Include only not-yet-reported crashes.
            opts=$(_abrt_list)
            ;;
        --delete)
            opts=$(_abrt_list "--full")
            ;;
    esac

    COMPREPLY=( $(compgen -W "${opts}" -- ${cur}) )
    return 0
}
complete -F _abrt_cli abrt-cli

# Local variables:
# mode: shell-script
# sh-basic-offset: 4
# sh-indent-comment: t
# indent-tabs-mode: nil
# End:
# ex: ts=4 sw=4 et filetype=sh