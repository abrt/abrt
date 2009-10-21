# bash-completion add-on for abrt-cli(1)
# http://bash-completion.alioth.debian.org/

_abrt_cli()
{
    local cur prev opts
    COMPREPLY=()
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"
    opts="--help --version --get-list --get-list-full --report --report-always --delete"

    #
    #  Complete the arguments to some of the basic commands.
    #
    case "${prev}" in
        --report|--report-always|--delete)
            local uuids=$(abrt-cli --get-list | grep UUID | awk '{print $3}')
            COMPREPLY=( $(compgen -W "${uuids}" -- ${cur}) )
            return 0
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