

   /**
    *  Gets URI that identifies the test.
    *  @return uri identifier of test
    */
function getTargetURI() {
      return "http://www.w3.org/2001/DOM-Test-Suite/level2/html/HTMLLinkElement11";
   }

var docsLoaded = 0;
var mainLoaded = false;
var cssLoaded = false;

function finishTest() {
    if (mainLoaded && cssLoaded) {
        setResult(null, null);
    } else {
        if (!cssLoaded) {
            setResult("fail", "No css onload fired");
        } else {
            setResult("fail", "Css fired, but maybe the document onload didn't");
        }
    }
    if (window.layoutTestController) {
        layoutTestController.notifyDone();
    }
}

function loadComplete() {
    mainLoaded = true;
    if (++docsLoaded == 2) {
        finishTest();
    }
}

function cssComplete() {
    cssLoaded = true;
    if (++docsLoaded == 2) {
        finishTest();
    }
}



function startTest11() {
    if (window.layoutTestController) {
        layoutTestController.dumpAsText();
        layoutTestController.waitUntilDone();
    }
}
