if( LatestVersionString == undefined)
{
  document.write( 
      '<strong><font color=#FF0000>' + 'You have version ' +ThisVersion + ',</font></strong> but ' +
      'this page was unable to connect to the web to check for version updates. ' +
      'You can inquire about updates manually by e-mailing by e-mailing <a class="paragraph" ' + 'href="mailto:'+ ProductEmail +
      '?subject=Check for updates since ' + ThisVersion + 
      '&body=(please fill in your company name, product name, and any other relevant license information here)' +
      '">'+ ProductEmail +'</a>.  If you do not have an internet connection, you can instead call '+ProductPhone+'.' );
}
else
{
  if(ThisVersion != LatestVersionString)
  {
    document.write( 
        '<strong><font color=#FF0000>' + 'A new version is available - you have '+ThisVersion+' and ' + LatestVersionString + ' is the latest! ' + '</font></strong>' +
        'You can <a class="paragraph" href="http://www.radgametools.com/msshist.htm">view the on-line changelog</a>, or request ' +
        'an update by e-mailing <a class="paragraph" ' + 'href="mailto:'+ ProductEmail +
        '?subject=Request for Miles Sound System version ' + LatestVersionString + 
        '&body=(please fill in your company name, product name, and any other relevant license information here)' +
        '">'+ ProductEmail +'</a>.' );
  }
  else
  {
    document.write( 'You have version ' + ThisVersion + ' and, according to the RAD website, this is the most recent release.' );
  }
}
