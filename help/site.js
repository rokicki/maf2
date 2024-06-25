var gAgent = navigator.userAgent.toLowerCase(); 
gbIE = (navigator.appName.indexOf("Microsoft") != -1);

{
  gbNav4 = (navigator.appName == "Netscape");
  gbIE4 = (navigator.appName.indexOf("Microsoft") != -1);
}

function msieversion()
{
  var msie = gAgent.indexOf("msie ");
  if (msie > 0)
    return gAgent.substring(msie+5,gAgent.indexOf(";",msie));
  return "";
}

var gbResize = gbIE4 && parseFloat(msieversion()) < 5.5;
var gbFixPre = gbIE4 && parseFloat(msieversion()) < 7.0;


if (gbFixPre)
  document.writeln("<STYLE type='text/css'> PRE { width:1ex;}</STYLE>");

if (location.protocol != 'http:' && location.protocol != 'https:')
  document.write('<div style="float:right"><a class="noprint" target=_blank href="http://maffsa.sourceforge.net/manpages'+location.href.substr(location.href.lastIndexOf('/'))+'">Find Online<\/a><\/div>');


/*
 *	function showlinks()
 *	Adds a navigation dropdown to the document, which gets
 *  populated by the data in the LINK tags of the document
 *  written by Christian Heilmann (http://icant.co.uk/)
 */
function showlinks()
{
// variables to change
	var elementid='linksnavigation'; 
	var elementtype='div';
	var dropdownlabel='Quick Jump to:';
	var dropdownbutton='jump';
	var dropdownid='linksnavigationdropdown';
// Check for DOM capabilities
	if(document.getElementById && document.createTextNode)
	{
// get all link elements of the current page and check if there are some
		pagelinks=document.getElementsByTagName('link');
		if(pagelinks.length>0)
		{
// creating local variables
			var ispage,linksgen,pagelinks,linksdiv,linksform,count;
			var linkslabel,linkssel,linkssubmit,newop,relatt,sel;
			var count=0; 
// check if the parent element already exists, if not, create it
			if(document.getElementById(elementid))
			{
				linksdiv=document.getElementById(elementid);
				linksgen=false;
			} else {
				linksgen=true;
				linksdiv=document.createElement(elementtype);
				linksdiv.setAttribute('id',elementid);
			}
// create dropdown form
			linksform=document.createElement('form');	
			linkslabel=document.createElement('label')
			linkslabel.appendChild(document.createTextNode(dropdownlabel));
			linkslabel.setAttribute('for',dropdownid);
			linkssel=document.createElement('select')
			linkssel.setAttribute('id',dropdownid);
			linkssubmit=document.createElement('input');
			linkssubmit.setAttribute('type','submit');
			linkssubmit.setAttribute('value',dropdownbutton);
// loop over link elements
			for(i=0;i<pagelinks.length;i++)
			{
// grab the rel attribute, and don't take any containing sheet
				relatt=pagelinks[i].getAttribute('rel');
				if(!/sheet/i.test(relatt)pagelinks[i].getAttribute('rel') &&
             pagelinks[i].getAttribute('title'))
				{
// create the dropdown options from the title and href attributes 
					newop=document.createElement('option');
					newop.appendChild(document.createTextNode(pagelinks[i].getAttribute('title')));
					newop.setAttribute('value',pagelinks[i].getAttribute('href'));
// check if the current location contains the href link
					if(document.location.href.indexOf(pagelinks[i].getAttribute('href'))!=-1){
						ispage=count;
					}
					linkssel.appendChild(newop)
					count++;
				}
			}	
// set the selection to the current page
			linkssel.selectedIndex=ispage?ispage:0;
			
// add the javascript to send the browser to the dropdown location
			linksform.onsubmit=function(){
				sel=document.getElementById(dropdownid);
				self.location=sel.options[sel.selectedIndex].value;
				return false;
			};
// assemble the form HTML and append it to the element
			linksform.appendChild(linkslabel);
			linksform.appendChild(linkssel);
			linksform.appendChild(linkssubmit);
			linksdiv.appendChild(linksform);
// add the element as the first body child if it is not there yet
			if(linksgen)
			{
				document.body.insertBefore(linksdiv,document.body.firstChild);
			}
		}
	}
}

function OnPageLoad()
{
  // Ensure images with no title get some kind of tooltip
  for (i= 0; i < document.images.length;i++)
  {
    im = document.images[i];
    if (im.title=="")
      im.title = im.alt;
  }
  // implement "niceborder" behavior - no longer using IE behaviors for this
  if (document.getElementsByTagName)
  {
    var tableCol = document.getElementsByTagName("TABLE");
    for (i = 0; i < tableCol.length;i++)
      if (tableCol[i].className == "niceborder")
      {
        var rowCol = tableCol[i].rows;
        for (j = 1; j < rowCol.length;j +=2)
          rowCol[j].className = "darkrow";
      }
  }
  showlinks();
}

function OnPageUnload()
{
}


function get_by_id(str)
{
  if (document.getElementById)
    return document.getElementById(str);
  return document.all[str];
}

function show_hide(id)
{
  var what = get_by_id_str(str)
  if (what.style.display == "none")
    what.style.display = "block";
  else
    what.style.display = "hidden";
}
