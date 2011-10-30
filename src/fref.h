/* $Id: fref.h 631 2007-03-30 17:14:06Z kyanh $ */
/* ---------------------------------------------------------- */
/* -------------- XML FILE FORMAT --------------------------- */
/* ---------------------------------------------------------- */

/* 
<ref name="MySQL Functions" description="MySQL functions for PHP " case="1">

<group name="some_function_group">
  <description>
     Group description
  </description>
      
        <group name="Groups can be nested">
           <function ...>
           </function>
        </group>

<function name="mysql_db_query">

   <description>
     Function description 
   </description>

   <tip>Text shown in a tooltip or hints</tip>
      
   <param name="database" title="Database name" required="1" vallist="0" default="" type="string" >
     <vallist>Values if vallist==1</vallist>
     Parameter description
   </param>       
    
   <return type="resource">
     Return value description
   </return>
  
   <dialog title="Dialog title">
      Text inserted after executing dialog, use params as %0,%1,%2 etc.
      %_ means "insert only these attributes which are not empty and not 
                default values" 
   </dialog>
   
   <insert>
      Text inserted after activating this action
   </insert>
   
   <info title="Title of the info window">    
     Text shown in the info
   </info>    
   
 </function>
 
 <tag name="Table Element">
 
   <description>
       The TABLE element contains all other elements that specify caption, rows, content, and formatting.
   </description>

   <tip>Tag tooltip</tip>
     
   <attribute name="Border" title="Table border" required="0" vallist="0" default="0">
      <vallist></vallist>
          This attribute specifies the width (in pixels only) of the frame around a table (see the Note below for more information about this attribute).
   </attribute>

   <dialog title="Dialog title">
      Text inserted after executing dialog, use params as %0,%1,%2 etc.
   </dialog>
   
   <insert>
      Text inserted after activating this action
   </insert>
   
   <info title="Title of the info window">    
     Text shown in the info
   </info>    
  
 </tag>
   
</group>

</ref>
*/

#ifndef __FREF_H__
#define __FREF_H__

GtkWidget *fref_gui(Tbfwin *bfwin); /* used in gui.c */

void fref_init(void); /* only used once */
void fref_cleanup(Tbfwin *bfwin);
void fref_rescan_dir(const gchar *dir); /* used by rcfile.c */

#endif /* __FREF_H__ */


