#! /bin/env bash

_chatty_complete()
{
    _init_completion || return

    local previous_previous=${COMP_WORDS[COMP_CWORD-2]}
    local previous=${COMP_WORDS[COMP_CWORD-1]}
    local current=${COMP_WORDS[COMP_CWORD]}
    local options="--retry --new-session= --prompt-from= --delete= --delete-all --list --export= --import= --rollback --help --session= --prompt= --once"
    local equals_options="--prompt-from --delete --export --import --session --prompt"

    if [[ "y$XDG_DATA_HOME" != "y" ]]; then
        local sessions=$(ls $XDG_DATA_HOME/chatty/sessions)
    else
        local sessions=$(ls ~/.local/share/chatty/sessions)
    fi

    # If current word is "=" and previous word is one of the equals options then
    # prepend an equals sign each of sessions and use those as completions
    if [[ "$current" == "=" && "$equals_options" =~ "$previous" ]] ; then
      if [[ "$previous" == "--prompt" ]] ; then
        # autocomplete with files
        COMPREPLY=( * )
      else 
        COMPREPLY=( $sessions )
      fi
      return 0
    fi

    # If previous word is "=" and previous previous word is one of the equals options then
    # just use the sessions as completions
    if [[ "$previous" == "=" && "$equals_options" =~ "$previous_previous" ]]; then
      if [[ "$previous_previous" == "--prompt" ]] ; then
        _filedir
      else
        COMPREPLY=( $(compgen -W "$sessions" -- $current) )
      fi
      return 0
    fi

    # Otherwise just use the options as completions
    COMPREPLY=( $(compgen -W "$options" -- $current) )
}

complete -o nospace -F _chatty_complete chatty
