#!/usr/bin/env Rscript
# This script can be run using:
# ./plot_multi_app.R <data> <synthetic_data>

library(ggplot2)
library(cowplot, warn.conflicts = FALSE)
library(sysfonts)
library(showtext)
library(showtextdb)
showtext_auto()

args <- commandArgs(trailingOnly=TRUE)
file <- args[1]
plot_filename <- args[2]
offload_file <- args[3]
offloading_data<-read.csv(offload_file)
offloading_data$Scheme <- factor(offloading_data$Scheme, levels=c("SP", "LR", "WF", "LND", "DCTCP_qdelay"), ordered=T)
limit <-200000
data<-read.csv(file)

label_list <- c("PS" = "Diffindo",
                 "SP" = "Shortest Path",
                 "LR" = "Landmark Routing",
                 "WF" = "Waterfilling", 
                 "DCTCP_qdelay" = "Spider",
                 "LND" = "LND",
                 "Circ" = "Circulation")

break_list <- c("PS", "SP", "LR", "WF", "LND","Circ", "DCTCP_qdelay")

color_list <- c(
                "SP" = "#e51a1c",
                "PS" = "#377eb8",
                "LR" = "#4daf4a",
                "WF" ="#984ea3",
                "LND" = "#fb9a99",
                "DCTCP_qdelay" = "#0c2c84",
                "Circ" = "#000000")

shape_list <- c(
                "PS" = 18,
                "LND" = 8,
                "LR" = 7,
                "SP" = 4,
                "WF" = 15,
                "DCTCP_qdelay" = 19,
                "Circ" = 17)

# add a line plot
succ_ratio_plot <- ggplot(data,
                          aes_string(x="RebalancingRate",y="SuccRatio", colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="SuccRatioMin", ymax="SuccRatioMax"),width=0.2) +
        scale_x_continuous(expand=c(0,0), trans="log10") +
        coord_cartesian(xlim=c(1,limit),ylim=c(0,100)) +
        labs(x="Rebalancing Interval (€)", y="Success Ratio (%)") +
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

        scale_linetype_manual(
                    labels=label_list,
                    breaks=break_list,
                    values=line_list,
                    guide=guide_legend(title=NULL, nrow =1)) +

        theme_minimal(base_size=22) +
        theme(axis.text.x=element_text(size=rel(1.3)), axis.text.y=element_text(size=rel(1.3))) + 
        theme(legend.text=element_text(size=rel(1)), legend.key.size=unit(15,"points"), legend.position="none",
              legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)))
ggsave(paste(plot_filename,"SuccRatio.pdf", sep=""), width=5.7, height=5)


# add a line plot
rebalancing_amt_plot <- ggplot(data, aes_string(x="RebalancingRate",y="SuccVolume", 
                                colour="Scheme", shape="Scheme")) +
        geom_line(size=1) +
        geom_point(size=3) +
        geom_errorbar(aes_string(ymin="SuccVolumeMin", ymax="SuccVolumeMax"),width=0.2) +
        scale_x_continuous(expand=c(0,0), trans="log10") +
        coord_cartesian(xlim=c(1,limit)) +
        labs(x="Rebalancing Interval (€)", y="Norm. Throughput (%)") +
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
        theme(axis.text.x=element_text(size=rel(1.3)), axis.text.y=element_text(size=rel(1.3))) + 
        theme(legend.text=element_text(size=rel(1)), legend.key.size=unit(15,"points"), legend.position="none",
              legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)))

ggsave(paste(plot_filename, "SuccVolume.pdf", sep=""), width=5.7, height=5)
legend <- get_legend(rebalancing_amt_plot + theme(legend.position="top"))

offloading_plot <- ggplot(offloading_data, aes_string(x="Topo", y="Offloading", fill="Scheme")) +
        geom_bar(stat="identity", position=position_dodge(), colour="black") +
        geom_errorbar(aes_string(ymin="OffloadingMin", ymax="OffloadingMax"), width=.3, 
                      position=position_dodge(.9)) +
        labs(x="Scheme", y="Offchain tx/onchain tx") +
        scale_fill_manual(
                values=color_list, 
                labels=label_list,
                breaks=break_list) +
        
        theme_minimal(base_size=22) +
        theme(axis.text.x=element_blank(), axis.text.y=element_text(size=rel(1.3)), axis.title.y=element_text(vjust=-2.5)) + 
        theme(legend.text=element_text(size=rel(1)), legend.key.size=unit(15,"points"), legend.position="none",
              legend.box.margin=margin(-10,-10,-10,-10),
              legend.margin=margin(c(0,0,0,0)))

prow <- plot_grid(succ_ratio_plot + theme(legend.position="none"),
                  rebalancing_amt_plot + theme(legend.position="none"), 
                  offloading_plot + theme(legend.position="none"), 
                  ncol = 3, align = "v", axis = "l")

# this tells it what order to put it in
# so basically tells it put legend first then plots with th legend height 20% of the 
# plot
p <- plot_grid(legend, prow, rel_heights=c(.2,1), ncol =1)

ggsave(paste(plot_filename,"Summary.pdf", sep=""), width=12.2, height=5)
