function HandleClick(e, menuname)
{
    if(!e) {var e = window.e;}
    if(window.event) {e.cancelBubble=true;} else {e.stopPropagation();}

    var whichone = undefined;
    if(menuname)
    {
        whichone = document.getElementById(menuname);
    }

    if(window.themenu)
    {
        if (whichone && (whichone.id == window.themenu.id))
        {
            whichone = undefined;
        }

        window.themenu.style.visibility="hidden";
    }

    window.themenu = whichone;

    if(window.themenu)
    {
        window.themenu.style.visibility = "visible";
    }

    imagetarget = e.target;
    if(!imagetarget) {imagetarget = e.srcElement;}
    if(imagetarget.className == "TOCGroup")
    {
        nested = imagetarget;
        if(nested && (nested.className != "TOCItem")) {nested = nested.nextSibling;}
        if(nested && (nested.className != "TOCItem")) {nested = nested.nextSibling;}
        if(nested && (nested.className != "TOCItem")) {nested = nested.nextSibling;}
        if(nested && (nested.className != "TOCItem")) {nested = nested.nextSibling;}
        if(!nested || (nested.className != "TOCItem")) {nested = imagetarget.parentNode;}
        if(nested && (nested.className != "TOCItem")) {nested = nested.nextSibling;}
        if(nested && (nested.className != "TOCItem")) {nested = nested.nextSibling;}
        if(nested && (nested.className != "TOCItem")) {nested = nested.nextSibling;}
        if(nested && (nested.className != "TOCItem")) {nested = nested.nextSibling;}
        if(nested && (nested.className == "TOCItem"))
        {
            if(nested.style.display=="none")
            {
                nested.style.display="";
                imagetarget.src="openbook_icon.jpg";
            }
            else
            {
                nested.style.display="none";
                imagetarget.src="closedbook_icon.jpg";
            }
        }
    }
}
