#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)
library(scales)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
data<-read.csv(file)

label_list <- c("PS" = "Diffindo",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "LND" = "Lightning Network",
                 "DCTCP" = "DCTCP (Queue)",
                 "DCTCP_qdelay" = "Spider",
                 "celer" = "Celer")

break_list <- c("PS", "celer", "SP", "LR", "WF", "LND", "DCTCP", "DCTCP_qdelay")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP" = "#a65628",
                "DCTCP_qdelay" = "#0c2c84",
                "celer" = "#ff7f00")

shape_list <- c(
                "PS" = 18,
                "LND" = 8,
                "LR" = 7,
                "SP" = 4,
                "WF" = 15,
                "DCTCP" = 17,
                "DCTCP_qdelay" = 19,
                "celer" = 10)



# add a line plot
succ_ratio_plot <- ggplot(data, aes_string(x="Credit",y="SuccRatio", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"), width=0.1) +
        scale_x_continuous(expand=c(0,0), trans="log10", breaks=c(100,200,400,800,1600,3200,6400,12800)) +
        coord_cartesian(ylim=c(0,100)) +
        labs(x="Mean Channel Size (€)", y="Success Ratio (%)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL, nrow=1)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        theme_minimal(base_size=22) + 
        theme(axis.text.x=element_text(size=rel(1.1)), axis.text.y=element_text(size=rel(1.3))) + 
        theme(legend.text=element_text(size=rel(1)), legend.key.size=unit(15,"points"), legend.position="none",
              legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)))
ggsave(paste(plot_filename,"SuccRatio.pdf", sep=""), width=5.7, height=5)
legend <- get_legend(succ_ratio_plot + theme(legend.position="top"))

# add a line plot
succ_volume_plot <- ggplot(data, aes_string(x="Credit",y="SuccVolume", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="SuccVolumeMin", ymax="SuccVolumeMax"), width=0.1) +
        scale_x_continuous(expand=c(0,0), trans="log10", breaks=c(100,200,400,800,1600,3200,6400,12800)) +
        coord_cartesian(ylim=c(0,100)) +
        labs(x="Mean Channel Size (€)", y="Norm. Throughput (%)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                guide=guide_legend(title=NULL)) +

        theme_minimal(base_size=22) + 
        theme(axis.text.x=element_text(size=rel(1.1)), axis.text.y=element_text(size=rel(1.3))) + 
ggsave(paste(plot_filename,"SuccVolume.pdf", sep=""), width=5.7, height=5)

# add a line plot for completion times
completion_time_plot <- ggplot(data, aes_string(x="Credit",y="CompTime", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="CompTimeMin", ymax="CompTimeMax"), width=0.1) +
        scale_x_continuous(expand=c(0,0), trans="log10", breaks=c(100,200,400,800,1600,3200,6400,12800)) +
        labs(x="Mean Channel Size (€)", y="Completion Time (ms)") +
        scale_colour_manual(
                values=color_list,
                labels=label_list,
                breaks=break_list,
                guide=guide_legend(title=NULL)) +

        scale_shape_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=shape_list,
                guide=guide_legend(title=NULL)) +

        theme_minimal(base_size=22) +         
        theme(axis.text.x=element_text(size=rel(1.1)), axis.text.y=element_text(size=rel(1.3))) 
ggsave(paste(plot_filename,"CompletionTime.pdf", sep=""), width=5.7, height=5)

prow <- plot_grid(succ_ratio_plot + theme(legend.position="none"),
                  succ_volume_plot + theme(legend.position="none"), 
                  #completion_time_plot + theme(legend.position="none"),
                  ncol = 2, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(legend, prow, rel_heights=c(.2,1), ncol =1)

ggsave(paste(plot_filename,"Summary.pdf", sep=""), width=12.2, height=5)




