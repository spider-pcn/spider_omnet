#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
plot_title <- args[3]
data<-read.csv(file)

if (file == "succRatioBarData") {
    plot_col <- "succRatio"
    y_axis <- "Success Ratio (%)"
    pos <- "top"
    height <- 3
} else {
    plot_col <- "succVolume"
    y_axis <- "Success Volume (%)"
    pos <- "none"
    height <- 2.5
}

label_list  <- c("HEP" = "Spider (Waterfilling)    ",
                 "SHP" = "Shortest Path    ",
                 "MF" = "Max-flow      ",
                 "SiW" = "SilentWhispers      ", 
                 "SM" = "SpeedyMurmurs       ", 
                 "BA" = "Spider (LP)       ")


break_list  <- c("HEP", "MF", "SHP", "BA", "SW", "SM")

color_list <- c(
                "BA" = "#e51a1c",
                "HEP" = "#377eb8",
                "MF" = "#4daf4a",
                "SHP" ="#984ea3",
                "SM" = "#fb9a99",
                "SiW" = "#ff7f00")

# add a line plot
succ_ratio_plot <- ggplot(data, aes_string(x="topology", y=plot_col, fill="scheme")) +
        geom_bar(stat="identity", position=position_dodge(), colour="black", width=.8) +
        labs(x="Topology", y=y_axis) +
        scale_fill_manual(values=color_list,
                          labels=label_list)+
        theme_minimal() +
        theme(legend.position=pos, legend.margin=margin(c(0,5,1,5)))+
        theme(legend.title=element_blank())


ggsave(plot_filename, width=6, height=height)
