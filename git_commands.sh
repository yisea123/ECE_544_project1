# bring remote changes to your local repository
$ git pull origin master

# push local changes to remote repository
$ git push origin master

# commit the files with an inline message
$ git commit -m "Insert a message here"

# remove all files in a folder from the repository
$ git rm --cached -r <folder>

# tag the commit to mark as a release point
$ git tag -a <tagname> -m "Insert a message here"

# push tags to remote repository
$ git push origin --tags

# delete tag from local repository
$ git tag -d <tagname>

# delete tag from remote repository
$ git push --delete origin <tagname>

# create a branch
$ git branch <branchname>

# switch branches
$ git checkout <branchname>

# display the log with a graph
$ git log --oneline --decorate --graph --all

# display the log in a clean format
$ git log --pretty=oneline

# display the log in condensed format
$ git shortlog

# show difference between staged and working environment
$ git diff

# show difference between staged and last commit
$ git diff --staged

# show difference between two branches
$ git diff <branchA> <branchB>

# merge another branch into current checked-out branch
$ git merge <anotherbranch>

# find which commit lines A thru B were altered in
$ git blame -L <##A>,<##B> <filename>

# undo current changes and revert to previous commit
$ git --no-edit revert HEAD~

# undo current changes and revert without creating a new commit
$ git --no-edit revert -n HEAD~

# check if a filename will be detected by the .gitignore file
$ git check-ignore -v -n --no-index <pathname>