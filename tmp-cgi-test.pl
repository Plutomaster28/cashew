$ENV{"REQUEST_METHOD"} = "GET";
$ENV{"CONTENT_LENGTH"} = 0;
$ENV{"QUERY_STRING"} = "action=list";
$ENV{"PATH_INFO"} = "/miyamii/cgi-bin/guestbook.pl";
do "./test-site/miyamii/cgi-bin/guestbook.pl";
print STDERR "ERR:$@\n";
