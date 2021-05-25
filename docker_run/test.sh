aif() {
    if [ $# -gt 0 ]; then
        echo $1
        return 1
    else
        echo $0
        return 0
    fi
}
if aif; then
    echo good
else
    echo bad
fi
